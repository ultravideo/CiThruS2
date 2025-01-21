#include "TrafficController.h"
#include "Entities/Car.h"
#include "Entities/Pedestrian.h"
#include "Areas/TrafficStopArea.h"
#include "EngineUtils.h"
#include "Misc/MathUtility.h"
#include "Misc/Debug.h"
#include "Entities/Tram/Tram.h"
#include "Parking/ParkingController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Runtime/Core/Public/Async/ParallelFor.h"

#include <list>

ATrafficController::ATrafficController()
{
 	// Set this actor to call Tick() every frame
	PrimaryActorTick.bCanEverTick = true;

#if WITH_EDITOR
	SetIsSpatiallyLoaded(false);
#endif

	SetActorHiddenInGame(true);
}

void ATrafficController::BeginPlay()
{
	Super::BeginPlay();

	// Configure detail control
	farDistance_ = distanceFromCameraToEnableLowDetail_;
	nearestEntities_.reserve(std::max(maxHighDetailVehiclesZeroForUnlimited_, 0));

	// Load keypoints
	roadGraph_.LoadFromFile(TCHAR_TO_UTF8(*(FPaths::ProjectDir() + "/DataFiles/roadGraph.data")));
	roadGraph_.AlignWithWorldGround(GetWorld());
	
	sharedUseGraph_.LoadFromFile(TCHAR_TO_UTF8(*(FPaths::ProjectDir() + "/DataFiles/sharedUseGraph.data")));
	sharedUseGraph_.AlignWithWorldGround(GetWorld());

	tramwayGraph_.LoadFromFile(TCHAR_TO_UTF8(*(FPaths::ProjectDir() + "/DataFiles/tramwayTrackGraph.data")));
	tramwayGraph_.AlignWithWorldGround(GetWorld());

	// Fetch traffic areas
	trafficAreas_.Empty();

	for (TActorIterator<AActor> it = TActorIterator<AActor>(GetWorld()); it; it.operator++())
	{
		if (ITrafficArea* trafficArea = Cast <ITrafficArea>(*it))
		{
			trafficAreas_.Add(trafficArea);
		}
		else if (AParkingController* ps = Cast<AParkingController>(*it))
		{
			parkingSystem_ = ps;
		}
	}

	// Apply extra rules to car keypoints from VehicleRuleZones, incase they aren't applied manually
	roadGraph_.ApplyZoneRules(this);

	// Get ref to parking controller
	TArray<AActor*> find;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AParkingController::StaticClass(), find);

	if (find.Num() <= 0)
	{
		Debug::Log("No ParkingController placed");
		return;
	}

	parkingController_ = Cast<AParkingController>(find[0]);

	BeginSimulateTraffic();
}

void ATrafficController::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

	for (TrafficEntityWrapper& entityWrapper : simulatedEntities_)
	{
		for (ITrafficArea* trafficArea : trafficAreas_)
		{
			trafficArea->UpdateCollisionStatusWithEntity(entityWrapper.entity);
		}

		if (visualizeCollisions_)
		{
			VisualizeCollisionForEntity(entityWrapper.entity, deltaTime);
		}
	}

	if (visualizeCollisions_)
	{
		for (ITrafficArea* trafficArea : trafficAreas_)
		{
			if (ATrafficStopArea* stopArea = Cast<ATrafficStopArea>(trafficArea)) 
			{
				// Visualize either red or green depending on traffic light state
				stopArea->GetCollisionRectangle().Visualize(GetWorld(), deltaTime, stopArea->Active() ? FColor::Red : FColor::Green);
			}
			else if (ARoadRegulationZone* ruleZone = Cast<ARoadRegulationZone>(trafficArea))
			{
				ruleZone->GetCollisionRectangle().Visualize(GetWorld(), deltaTime, FColor::Yellow);
			}
			else
			{
				trafficArea->GetCollisionRectangle().Visualize(GetWorld(), deltaTime, FColor::Blue);
			}		
		}

		std::list<APawn*> pawns;

		// Collect current pawns in the scene
		for (TActorIterator<APawn> it = TActorIterator<APawn>(GetWorld()); it; it.operator++())
		{
			if (*it == nullptr)
			{
				continue;
			}

			pawns.push_back(*it);
		}
		
		for (APawn* pawn : pawns)
		{
			FBox box = pawn->CalculateComponentsBoundingBoxInLocalSpace(false, false);
			Debug::DrawTemporaryRect(GetWorld(), pawn->GetActorLocation(), pawn->GetActorQuat().Vector(), box.GetSize().X, box.GetSize().Y, box.GetSize().Z, 20.0f, FColor::Orange, deltaTime * 1.1f);
		}
	}

	CheckEntityCollisions();
}

void ATrafficController::CheckEntityCollisions()
{
	std::list<CollisionRectangle> pawnCollisions;

	// Collect current pawns in the scene
	for (TActorIterator<APawn> it = TActorIterator<APawn>(GetWorld()); it; it.operator++())
	{
		if (*it == nullptr)
		{
			continue;
		}

		// Calculate pawns collision bounds
		FBox box = it->CalculateComponentsBoundingBoxInLocalSpace(false, false);
		CollisionRectangle pawnCollision = CollisionRectangle(box.GetSize(), it->GetActorLocation(), it->GetActorQuat());
		pawnCollisions.push_back(pawnCollision);
	}

	FMinimalViewInfo cameraViewInfo;
	FMatrix cameraViewMatrix;
	float cameraTanInverse;
	float aspectRatioInverse;
	UWorld* world = GetWorld();

	if (world != nullptr)
	{
		cameraViewInfo = world->GetFirstPlayerController()->PlayerCameraManager->GetCameraCacheView();
		FMatrix cameraMatrix = FTransform(cameraViewInfo.Rotation, cameraViewInfo.Location).ToMatrixNoScale();
		cameraViewMatrix = FTransform(cameraViewInfo.Rotation, cameraViewInfo.Location).Inverse().ToMatrixNoScale();
		aspectRatioInverse = 1.0f / cameraViewInfo.AspectRatio;
		cameraTanInverse = 1.0f / tan(FMath::DegreesToRadians(cameraViewInfo.FOV / 2.0f));

		if (visualizeViewFrustrum_)
		{
			float nearPlane = 100.0f;
			float farPlane = farDistance_;

			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(nearPlane, nearPlane / cameraTanInverse, (nearPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, farPlane / cameraTanInverse, (farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);
			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(nearPlane, -nearPlane / cameraTanInverse, (nearPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, -farPlane / cameraTanInverse, (farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);
			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(nearPlane, -nearPlane / cameraTanInverse, -(nearPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, -farPlane / cameraTanInverse, -(farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);
			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(nearPlane, nearPlane / cameraTanInverse, -(nearPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, farPlane / cameraTanInverse, -(farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);

			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(farPlane, farPlane / cameraTanInverse, (farPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, -farPlane / cameraTanInverse, (farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);
			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(farPlane, -farPlane / cameraTanInverse, (farPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, -farPlane / cameraTanInverse, -(farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);
			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(farPlane, -farPlane / cameraTanInverse, -(farPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, farPlane / cameraTanInverse, -(farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);
			Debug::DrawTemporaryLine(world, cameraMatrix.TransformPosition(FVector(farPlane, farPlane / cameraTanInverse, -(farPlane * aspectRatioInverse) / cameraTanInverse)), cameraMatrix.TransformPosition(FVector(farPlane, farPlane / cameraTanInverse, (farPlane * aspectRatioInverse) / cameraTanInverse)), FColor::Purple, 0.2f);
		}
	}

	nearestEntities_.clear();
	entityInFront_ = nullptr;
	entityInFrontDistance_ = std::numeric_limits<float>::max();

	//for (int i = 0; i < vehiclesInScene.Num(); i++)
	ParallelFor(simulatedEntities_.size(), [&](int32 i)
	{
		TrafficEntityWrapper& entityWrapper = simulatedEntities_[i];

		// All collisions should be cleared first to avoid the issue that a traffic entity is deleted but
		// other traffic entities are still referencing it
		entityWrapper.entity->ClearBlockingCollisions();

		// Check if this vehicle collides with other entities
		for (int j = 0; j < simulatedEntities_.size(); j++)
		{
			// Don't check against self
			if (i == j)
			{
				continue;
			}

			ITrafficEntity* otherEntity = simulatedEntities_[j].entity;

			entityWrapper.entity->UpdateBlockingCollisionWith(otherEntity);
		}

		// Check if this vehicle collides with pawns
		for (CollisionRectangle pawnCollision : pawnCollisions)
		{
			entityWrapper.entity->UpdatePawnCollision(pawnCollision);
		}

		if (!pawnCollisions.empty())
		{
			FVector pawnForward = pawnCollisions.front().GetRotation() * FVector::UnitX();
			FVector entityRelativeToPawn = entityWrapper.entity->GetCollisionRectangle().GetPosition() - pawnCollisions.front().GetPosition();

			if (FVector::DotProduct(pawnForward, entityRelativeToPawn.GetSafeNormal()) > 0.8f && pawnCollisions.front().GetRotation().AngularDistance(entityWrapper.entity->GetCollisionRectangle().GetRotation()) < UE_PI / 3.0f)
			{
				float entityDistance = entityRelativeToPawn.SquaredLength();

				entityInFrontMutex_.lock();

				if (entityDistance < entityInFrontDistance_)
				{
					entityInFront_ = entityWrapper.entity;
					entityInFrontDistance_ = entityDistance;
				}

				entityInFrontMutex_.unlock();
			}
		}

		// Update entity distance from camera
		entityWrapper.distanceFromCamera = FVector::Dist(cameraViewInfo.Location, entityWrapper.entity->GetCollisionRectangle().GetPosition());

		// Check if entity is in camera view
		std::vector<FVector> corners = entityWrapper.entity->GetCollisionRectangle().GetCorners();

		FVector bboxMin;
		FVector bboxMax;

		bool bboxInitialized = false;

		// Calculate the bounding box of the object's collision rectangle in the camera view
		if (lowDetailVehiclesOutsideCamera_)
		{
			for (int j = 0; j < 8; j++)
			{
				FVector4 posInCameraSpace = cameraViewMatrix.TransformPosition(corners[j]);

				if (posInCameraSpace.X <= 0)
				{
					continue;
				}

				FVector posProjected = FVector(
					posInCameraSpace.Y * cameraTanInverse / std::abs(posInCameraSpace.X),
					posInCameraSpace.Z * cameraTanInverse / std::abs(posInCameraSpace.X * aspectRatioInverse),
					posInCameraSpace.X / farDistance_);

				if (!bboxInitialized)
				{
					bboxMin = posProjected;
					bboxMax = posProjected;

					bboxInitialized = true;
				}
				else
				{
					if (posProjected.X < bboxMin.X)
					{
						bboxMin.X = posProjected.X;
					}

					if (posProjected.Y < bboxMin.Y)
					{
						bboxMin.Y = posProjected.Y;
					}

					if (posProjected.Z > bboxMax.Z)
					{
						bboxMax.Z = posProjected.Z;
					}

					if (posProjected.X > bboxMax.X)
					{
						bboxMax.X = posProjected.X;
					}

					if (posProjected.Y > bboxMax.Y)
					{
						bboxMax.Y = posProjected.Y;
					}

					if (posProjected.Z > bboxMax.Z)
					{
						bboxMax.Z = posProjected.Z;
					}
				}
			}
		}

		if (entityWrapper.distanceFromCamera < farDistance_
			&& !lowDetailVehiclesOutsideCamera_
			|| (bboxInitialized
				&& bboxMin.X < 1.0f && bboxMin.Y < 1.0f && bboxMin.Z < 1.0f
				&& bboxMax.X > -1.0f && bboxMax.Y > -1.0f && bboxMax.Z > 0.0f))
		{
			if (maxHighDetailVehiclesZeroForUnlimited_ <= 0)
			{
				entityWrapper.isNear = true;
			}
			else
			{
				nearestEntitiesMutex_.lock();

				static auto distanceLessComparer = [](const TrafficEntityWrapper* a, const TrafficEntityWrapper* b) { return a->distanceFromCamera < b->distanceFromCamera; };

				// Gather a heap of the n closest traffic entities. A heap is efficient because we do not need to know the exact distances,
				// only whether they're smaller than the maximum distance. This avoids extra sorting
				if (nearestEntities_.size() < maxHighDetailVehiclesZeroForUnlimited_)
				{
					nearestEntities_.push_back(&entityWrapper);
					std::push_heap(nearestEntities_.begin(), nearestEntities_.end(), distanceLessComparer);
					entityWrapper.isNear = true;
				}
				else if (distanceLessComparer(&entityWrapper, nearestEntities_.front()))
				{
					std::pop_heap(nearestEntities_.begin(), nearestEntities_.end(), distanceLessComparer);
					nearestEntities_.back()->isNear = false;
					nearestEntities_.back() = &entityWrapper;
					std::push_heap(nearestEntities_.begin(), nearestEntities_.end(), distanceLessComparer);
					entityWrapper.isNear = true;
				}
				else
				{
					entityWrapper.isNear = false;
				}

				nearestEntitiesMutex_.unlock();
			}
		}
		else
		{
			entityWrapper.isNear = false;
		}
	});

	ParallelFor(staticEntities_.size(), [&](int32 i)
	{
		TrafficEntityWrapper& entityWrapper = staticEntities_[i];

		if (!pawnCollisions.empty())
		{
			FVector pawnForward = pawnCollisions.front().GetRotation() * FVector::UnitX();
			FVector entityRelativeToPawn = entityWrapper.entity->GetCollisionRectangle().GetPosition() - pawnCollisions.front().GetPosition();

			if (FVector::DotProduct(pawnForward, entityRelativeToPawn.GetSafeNormal()) > 0.8f && pawnCollisions.front().GetRotation().AngularDistance(entityWrapper.entity->GetCollisionRectangle().GetRotation()) < UE_PI / 3.0f)
			{
				float entityDistance = entityRelativeToPawn.SquaredLength();

				entityInFrontMutex_.lock();

				if (entityDistance < entityInFrontDistance_)
				{
					entityInFront_ = entityWrapper.entity;
					entityInFrontDistance_ = entityDistance;
				}

				entityInFrontMutex_.unlock();
			}
		}
	});

	// Tell entities if they've changed state
	// This would be more efficient in a ParallelFor but it crashes for some reason
	for (int i = 0; i < simulatedEntities_.size(); i++)
	{
		if (simulatedEntities_[i].isNear && !simulatedEntities_[i].previousIsNear)
		{
			simulatedEntities_[i].entity->OnNear();
		}
		else if (!simulatedEntities_[i].isNear && simulatedEntities_[i].previousIsNear)
		{
			simulatedEntities_[i].entity->OnFar();
		}

		simulatedEntities_[i].previousIsNear = simulatedEntities_[i].isNear;
	}
}

TArray<AActor*> ATrafficController::GetEntitiesInArea(FVector center3d, FVector forward3d, float length, float width)
{
	TArray<AActor*> colliding;

	FVector2D center = FVector2D(center3d);
	FVector2D forward = FVector2D(forward3d);
	FVector2D right = FVector2D(forward3d.ToOrientationQuat().GetRightVector());
	FVector2D av = FVector2D(center + 0.5f * (forward * length + right * width));
	FVector2D bv = FVector2D(center + 0.5f * (forward * length - right * width));
	FVector2D cv = FVector2D(center + 0.5f * (-forward * length - right * width));
	FVector2D dv = FVector2D(center + 0.5f * (-forward * length + right * width));

	// Collect current pawns in the scene
	std::list<APawn*> pawns;
	for (TActorIterator<APawn> it = TActorIterator<APawn>(GetWorld()); it; it.operator++())
	{
		if (*it == nullptr)
		{
			continue;
		}

		pawns.push_back(*it);
	}
	// Check against cars?
	for (int i = 0; i < simulatedEntities_.size(); i++)
	{
		if (ACar* car = Cast<ACar>(simulatedEntities_[i].entity))
		{
			if (MathUtility::PointInsideRectangle(FVector2D(car->GetActorLocation()), av, bv, cv, dv))
			{
				colliding.Add(car);
			}
		}
	}

	// Check against pawns
	for (APawn* pawn : pawns)
	{
		if (MathUtility::PointInsideRectangle(FVector2D(pawn->GetActorLocation()), av, bv, cv, dv))
		{
			colliding.Add(pawn);
		}
	}

	return colliding;
}

ACar* ATrafficController::SpawnCar()
{
	int spawnPoint = roadGraph_.GetRandomSpawnPoint();

	return SpawnCar(roadGraph_.GetKeypointPosition(spawnPoint), roadGraph_.GetKeypointRotation(spawnPoint), true, templateCars_[FMath::RandRange(0, templateCars_.Num() - 1)], -1);
}

ACar* ATrafficController::SpawnCar(const FVector& position, const FRotator& rotation, const bool& simulate)
{
	return SpawnCar(position, rotation, simulate, templateCars_[FMath::RandRange(0, templateCars_.Num() - 1)], -1);
}

ACar* ATrafficController::SpawnCar(const FVector& position, const FRotator& rotation, const bool& simulate, const TSubclassOf<ACar>& carClass, const int& carVariant)
{
	if (templateCars_.Num() == 0)
	{
		Debug::Log("Can't spawn car: no template cars available!");
		return nullptr;
	}

	FActorSpawnParameters spawnParams;

	spawnParams.SpawnCollisionHandlingOverride
		= ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ACar* car = GetWorld()->SpawnActor<ACar>(carClass, position, rotation, spawnParams);

	if (!car)
	{
		Debug::Log("Car spawn failed");
		return nullptr;
	}

	if (carVariant >= 0)
	{
		car->SetVariantId(carVariant);
	}

#if WITH_EDITOR
	car->SetFolderPath(FName("TrafficSystems/Vehicles"));
#endif

	if (simulate)
	{
		simulatedEntities_.push_back({ car, false });
		car->SetController(this);
		car->Simulate(&roadGraph_);
	}
	else
	{
		staticEntities_.push_back({ car, false });
	}

	return car;
}

APedestrian* ATrafficController::SpawnPedestrian(const FVector& position, const FRotator& rotation, const bool& simulate)
{
	if (templatePedestrians_.Num() == 0)
	{
		Debug::Log("Can't spawn pedestrian: no template pedestrians available!");
		return nullptr;
	}

	FActorSpawnParameters spawnParams;

	spawnParams.SpawnCollisionHandlingOverride
		= ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APedestrian* pedestrian = GetWorld()->SpawnActor<APedestrian>(
		templatePedestrians_[FMath::RandRange(0, templatePedestrians_.Num() - 1)],
		position + APedestrian::PreferredSpawnPositionOffset(), rotation, spawnParams);

	if (!pedestrian)
	{
		Debug::Log("Pedestrian spawn failed");
		return nullptr;
	}

#if WITH_EDITOR
	pedestrian->SetFolderPath(FName("TrafficSystems/Pedestrians"));
#endif

	if (simulate)
	{
		simulatedEntities_.push_back({ pedestrian, false });
		pedestrian->SetController(this);
		pedestrian->Simulate(&sharedUseGraph_);
	}
	else
	{
		staticEntities_.push_back({ pedestrian, false });
	}

	return pedestrian;
}

ATram* ATrafficController::SpawnTram(const FVector& position, const bool& simulate)
{
	if (templateTrams_.Num() == 0)
	{
		Debug::Log("Can't spawn tram: no template trams available!");
		return nullptr;
	}

	FActorSpawnParameters spawnParams;

	spawnParams.SpawnCollisionHandlingOverride
		= ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ATram* tram = GetWorld()->SpawnActor<ATram>(
		templateTrams_[FMath::RandRange(0, templateTrams_.Num() - 1)], position, FRotator(), spawnParams);

	if (!tram)
	{
		Debug::Log("Car spawn failed");
		return nullptr;
	}

#if WITH_EDITOR
	tram->SetFolderPath(FName("TrafficSystems/Trams"));
#endif
	
	if (simulate)
	{
		simulatedEntities_.push_back({ tram, false });
		tram->SetController(this);
		tram->Simulate(&tramwayGraph_);
	}
	else
	{
		staticEntities_.push_back({ tram, false });
	}

	return tram;
}

void ATrafficController::RespawnCar(ACar* car)
{
	car->Destroy();

	// Try to spawn from a parking space 50% of the time
	if (FMath::RandRange(0.0f, 1.0f) > 0.5f && parkingController_->DepartRandomParkedCar())
	{
		return;
	}
	
	SpawnCar();
}

void ATrafficController::InvalidateTrafficEntity(ITrafficEntity* entity)
{
	if (entity != nullptr && !massDeletionInProgress_)
	{
		auto it = std::find_if(simulatedEntities_.begin(), simulatedEntities_.end(), [entity](TrafficEntityWrapper wrapper) { return wrapper.entity == entity; });
		
		if (it != simulatedEntities_.end())
		{
			simulatedEntities_.erase(it);
		}
	}
}

void ATrafficController::BeginSimulateTraffic()
{
	DeleteAllEntities();

	// Create new cars
	
	// Somewhat close margin based on the amount of cars, to prevent repeating keypoints, and having as much space as possible for each car 
	float kpMargin = 10000000 * FMath::Pow(amountOfCarsToSpawn_, -1.4) + 300;
	kpMargin = FMath::Clamp(kpMargin, 0, 4000);

	std::vector<int> keypointsWithMargin = roadGraph_.GetKeypointsWithMargin(kpMargin); 
	// Remove intersections from spawn location list, to prevent cars getting stuck in those
	roadGraph_.RemoveKeypointsByRange(keypointsWithMargin, FVector2D(-12060.0, -45180.0), 2600.0f); // Campus intersection
	roadGraph_.RemoveKeypointsByRange(keypointsWithMargin, FVector2D(-17850.0, -73060.0), 2250.0f); // Duo intersection
	roadGraph_.RemoveKeypointsByRange(keypointsWithMargin, FVector2D(-23510.0, -84480.0), 1975.0f); // Polamk intersection
	roadGraph_.RemoveKeypointsByRange(keypointsWithMargin, FVector2D(-38430.0, -108170.0), 2150.0f); // Neste intersection
	roadGraph_.RemoveKeypointsByRange(keypointsWithMargin, FVector2D(-52780.0, -160660.0), 3400.0f); // Valtavaeylae intersection
	roadGraph_.RemoveKeypointsByRange(keypointsWithMargin, FVector2D(22100.0, 19480), 3275.0f); // That one intersection always causing trouble

	// Spawn entities at "random" keypoints while avoiding spawning at the same keypoint twice. Index
	// selection is based on this blog post. 524287 is just an arbitrary big prime
	// https://lemire.me/blog/2017/09/18/visiting-all-values-in-an-array-exactly-once-in-random-order/
	const int prime = 524287;
	int randomSeed = FMath::RandRange(0, prime - 1);

	if (keypointsWithMargin.size() > 0)
	{
		int spawnNumber = 0; // Keep a separate counter to avoid spawning twice with the same one

		for (int i = 0; i < amountOfCarsToSpawn_; i++)
		{
			// Spawn car at random keypoint, Enforce spawning by keypoint rules
			TSubclassOf<ACar> templateCar = templateCars_[FMath::RandRange(0, templateCars_.Num() - 1)];
			int32 ruleExceptions = templateCar.GetDefaultObject()->GetKeypointRuleExceptions();

			int fs = 0;

			while (fs < 50)
			{
				int kpIndex = (spawnNumber++ * prime + randomSeed) % keypointsWithMargin.size();

				if (roadGraph_.CompareRules(kpIndex, ruleExceptions))
				{
					SpawnCar(roadGraph_.GetKeypointPosition(keypointsWithMargin[kpIndex]), roadGraph_.GetKeypointRotation(keypointsWithMargin[kpIndex]), true);
					break;
				}

				fs++;
			}		
		}
	}
	else
	{
		Debug::Log("no road keypoints, can't spawn cars!");
	}

	 // Create new trams

	if (tramwayGraph_.KeypointCount() > 0)
	{
		if (amountOfTramsToSpawn_ > 0)
		{
			// Spawn the trams evenly across the track
			int offset = FMath::Floor(FMath::RandRange(0, tramwayGraph_.KeypointCount() - 1) / amountOfTramsToSpawn_);

			for (int i = 0; i < amountOfTramsToSpawn_; i++)
			{
				int spawnIndex = FMath::Floor(tramwayGraph_.KeypointCount() * (float)i / (float)amountOfTramsToSpawn_) + offset;

				if (spawnIndex >= tramwayGraph_.KeypointCount())
				{
					spawnIndex -= tramwayGraph_.KeypointCount();
				}

				SpawnTram(tramwayGraph_.GetKeypointPosition(spawnIndex), true);
			}
		}	
	}
	else
	{
		Debug::Log("No tramway keypoints, can't spawn trams!");
	}

	// Create new pedestrians

	if (sharedUseGraph_.KeypointCount() > 0)
	{
		for (int i = 0; i < amountOfPedestriansToSpawn_; i++)
		{
			// Spawn pedestrian at random keypoint
			int kpIndex = (i * prime + randomSeed) % sharedUseGraph_.KeypointCount();
			const FVector position = sharedUseGraph_.GetKeypointPosition(kpIndex);

			SpawnPedestrian(position, FRotator::ZeroRotator, true);
		}
	}
	else
	{
		Debug::Log("no shared use keypoints, can't spawn pedestrians!");
	}
}

template<class T>
void ATrafficController::DeleteAllEntitiesOfType()
{
	massDeletionInProgress_ = true;

	// Remove and destroy all entities that can be cast into the given type
	auto predicate = [](TrafficEntityWrapper entityWrapper)
	{
		T* castEntity = Cast<T>(entityWrapper.entity);

		if (castEntity != nullptr)
		{
			// If you get errors complaining about this spot it's because the
			// type you gave to the template doesn't have a Destroy() function
			castEntity->Destroy();

			return true;
		}

		return false;
	};

	for (auto it = std::find_if(simulatedEntities_.begin(), simulatedEntities_.end(), predicate);
		it != simulatedEntities_.end();
		it = std::find_if(it, simulatedEntities_.end(), predicate))
	{
		it = simulatedEntities_.erase(it);
	}

	for (auto it = std::find_if(staticEntities_.begin(), staticEntities_.end(), predicate);
		it != staticEntities_.end();
		it = std::find_if(it, staticEntities_.end(), predicate))
	{
		it = staticEntities_.erase(it);
	}

	massDeletionInProgress_ = false;
}

void ATrafficController::VisualizeCollisionForEntity(ITrafficEntity* entity, const float& deltaTime) const
{
	entity->GetCollisionRectangle().Visualize(GetWorld(), deltaTime);
	entity->GetPredictedFutureCollisionRectangle().Visualize(GetWorld(), deltaTime, FColor::Blue);

	const FVector rayBegin = entity->GetCollisionRectangle().GetPosition();

	const FVector rayEnd = rayBegin + entity->GetMoveDirection() * 250.0f;

	Debug::DrawTemporaryLine(GetWorld(), rayBegin, rayEnd, FColor::Blue, deltaTime * 1.1f, 35.0f);
}

void ATrafficController::ClearLinkLines()
{
	Debug::DeletePersistentLines(GetWorld());
}

void ATrafficController::RedrawLinkLines()
{
	ClearLinkLines();

	VisualizeGraph(&roadGraph_);
	VisualizeGraph(&sharedUseGraph_);
	VisualizeGraph(&tramwayGraph_);
}

void ATrafficController::VisualizeGraph(KeypointGraph* graph) const
{
	for (int i = 0; i < graph->LinkCount(); i++)
	{
		std::pair<int, int> link = graph->GetLinkKeypoints(i);

		FVector direction = (graph->GetKeypointPosition(link.second) - graph->GetKeypointPosition(link.first));
		direction.Normalize();

		Debug::DrawPersistentLine(
			GetWorld(),
			graph->GetKeypointPosition(link.first),
			graph->GetKeypointPosition(link.second),
			FColor::Green,
			15.0f);

		// directional indicator
		Debug::DrawPersistentLine(
			GetWorld(),
			graph->GetKeypointPosition(link.second) - direction * 100,
			graph->GetKeypointPosition(link.second),
			FColor::Red,
			15.f);
	}
}

void ATrafficController::AddNewRegulationZone()
{
#if WITH_EDITOR
	// Spawn actor
	ARoadRegulationZone* zone = GetWorld()->SpawnActor<ARoadRegulationZone>();
	zone->SetFolderPath("/Traffic/RoadRegulationZones/Zones");

	// Get camera location
	FVector spawnLocation;

	if (UWorld* world = GetWorld())
	{
		auto viewLocations = world->ViewLocationsRenderedLastFrame;

		if (viewLocations.Num() == 0)
		{
			return;
		}

		spawnLocation = viewLocations[0];
	}

	// Set actor position
	zone->SetActorTransform(FTransform(FRotator::ZeroRotator, spawnLocation, FVector(5.0f, 5.0f, 5.0f)));

	hideRegulationZones_ = false;
	ApplyRegulationZoneVisibility();
#endif
}

void ATrafficController::ApplyRegulationZoneVisibility()
{
	for (ARoadRegulationZone* comp : GetAllRegulationZones())
	{
		comp->GetRootComponent()->SetVisibility(!hideRegulationZones_);
		comp->ApplySettings();
	}
}

FZoneRules ATrafficController::GetApplyingRegulationRulesAtPoint(FVector point)
{
	TArray<ARoadRegulationZone*> zones = GetAllRegulationZones();
	TArray<FZoneRules> inside;

	for (int i = 0; i < zones.Num(); i++)
	{
		ARoadRegulationZone* current = zones[i];
		// Check point is inside
		if (!current->GetCollisionRectangle().IsIntersecting(FVector2D(point)))
		{
			continue;
		}
		else
		{
			inside.Add(current->GetAllRules());
		}
	}

	// In case no zone overlaps point, return default rules.
	if (inside.Num() == 0)
	{
		return defaultRegulationZone_;
	}

	// Merge rules together, see FZoneRules on what takes priority
	FZoneRules ret = defaultRegulationZone_;

	for (FZoneRules rule : inside)
	{
		ret = ret + rule;
	}

	return ret;
}

TArray<ARoadRegulationZone*> ATrafficController::GetAllRegulationZones() const
{
	TArray<ARoadRegulationZone*> ret;
	TArray<AActor*> found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARoadRegulationZone::StaticClass(), found);

	for (AActor* f : found)
	{
		ARoadRegulationZone* comp = Cast<ARoadRegulationZone>(f);

		if (IsValid(comp))
		{
			ret.Add(comp);
		}
	}

	return ret;
}

void ATrafficController::EDITOR_InitRegulationCollisions()
{
	for (ARoadRegulationZone* zone : GetAllRegulationZones())
	{
		zone->EDITOR_InitCollisions();
	}
}
