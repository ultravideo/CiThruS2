#include "TrafficController.h"
#include "NPCar.h"
#include "Pedestrian.h"
#include "TrafficStopArea.h"
#include "PlayerCarPawn.h"
#include "EngineUtils.h"
#include "../MathUtility.h"
#include "../Debug.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Runtime/Core/Public/Async/ParallelFor.h"

#include <queue>
#include <list>

ATrafficController::ATrafficController()
{
 	// Set this actor to call Tick() every frame
	PrimaryActorTick.bCanEverTick = true;
}

void ATrafficController::BeginPlay()
{
	Super::BeginPlay();

	if (disableAllNpcs_)
	{
		return;
	}

	// Load keypoints
	roadGraph_.LoadFromFile(TCHAR_TO_UTF8(*(FPaths::ProjectDir() + "roadGraph.data")));
	roadGraph_.AlignWithWorldGround(GetWorld());

	sharedUseGraph_.LoadFromFile(TCHAR_TO_UTF8(*(FPaths::ProjectDir() + "sharedUseGraph.data")));
	sharedUseGraph_.AlignWithWorldGround(GetWorld());

	// Fetch traffic areas
	trafficAreas_.Empty();

	for (TActorIterator<AActor> it = TActorIterator<AActor>(GetWorld()); it; it.operator++())
	{
		ITrafficArea* trafficArea = Cast<ITrafficArea>(*it);

		if (trafficArea != nullptr)
		{
			trafficAreas_.Add(trafficArea);
		}
	}

	BeginSimulateTraffic();
}

void ATrafficController::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

	if (disableAllNpcs_)
	{
		return;
	}

	CleanupInvalidTrafficEntities();

	for (ANPCar* car : simulatedVehicles_)
	{
		for (ITrafficArea* trafficArea : trafficAreas_)
		{
			trafficArea->UpdateCollisionStatusWithEntity(car);
		}

		if (visualizeCollisions_)
		{
			VisualizeCollisionForEntity(car, deltaTime);
		}
	}

	for (APedestrian* pedestrian : pedestrians_)
	{
		for (ITrafficArea* trafficArea : trafficAreas_)
		{
			trafficArea->UpdateCollisionStatusWithEntity(pedestrian);
		}

		if (visualizeCollisions_)
		{
			VisualizeCollisionForEntity(pedestrian, deltaTime);
		}
	}

	if (visualizeCollisions_)
	{
		for (ITrafficArea* trafficArea : trafficAreas_)
		{
			trafficArea->GetCollisionRectangle().Visualize(GetWorld(), deltaTime);
		}
	}

	CheckCarCollisions();
}

void ATrafficController::CheckCarCollisions()
{
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

	//for (int i = 0; i < vehiclesInScene.Num(); i++)
	ParallelFor(simulatedVehicles_.Num(), [&](int32 i)
	{
		ANPCar* a = simulatedVehicles_[i];

		// Check if this car collides with other cars
		for (int j = 0; j < simulatedVehicles_.Num(); j++)
		{
			// Don't check against self
			if (i == j)
			{
				continue;
			}

			ANPCar* b = simulatedVehicles_[j];

			if (a->CollidingWith(b))
			{
				a->AddBlockingActor(b);
			}
			else
			{
				a->RemoveBlockingActor(b);
			}
		}

		// Check if this car collides with pedestrians
		for (int j = 0; j < pedestrians_.Num(); j++)
		{
			if (a->CollidingWith(pedestrians_[j]))
			{
				a->AddBlockingActor(pedestrians_[j]);
			}
			else
			{
				a->RemoveBlockingActor(pedestrians_[j]);
			}
		}

		// Check if this car collides with pawns
		for (APawn* pawn : pawns)
		{
			if (a->CollidingWith(pawn->GetActorLocation()))
			{
				a->AddBlockingActor(pawn);
			}
			else
			{
				a->RemoveBlockingActor(pawn);
			}
		}
	});
}

void ATrafficController::CleanupInvalidTrafficEntities()
{
	// This function is needed because traffic entities may be deleted
	// at unexpected times by Unreal Engine (z kill plane), the user
	// through the editor or future CiThruS code. This deletion leaves
	// invalid pointers in the traffic entity lists here which must be
	// removed to avoid dereferencing them and crashing.

	std::queue<ANPCar*> invalidSimulatedVehicles;
	std::queue<ANPCar*> invalidStaticVehicles;
	std::queue<APedestrian*> invalidPedestrians;

	// Gather invalid traffic entities
	for (ANPCar* car : simulatedVehicles_)
	{
		if (!IsValid(car))
		{
			invalidSimulatedVehicles.push(car);
		}
	}

	for (ANPCar* car : staticVehicles_)
	{
		if (!IsValid(car))
		{
			invalidStaticVehicles.push(car);
		}
	}

	for (APedestrian* pedestrian : pedestrians_)
	{
		if (!IsValid(pedestrian))
		{
			invalidPedestrians.push(pedestrian);
		}
	}

	// Remove invalid traffic entities from lists
	while (!invalidSimulatedVehicles.empty())
	{
		simulatedVehicles_.Remove(invalidSimulatedVehicles.front());
		invalidSimulatedVehicles.pop();
	}

	while (!invalidStaticVehicles.empty())
	{
		staticVehicles_.Remove(invalidStaticVehicles.front());
		invalidStaticVehicles.pop();
	}

	while (!invalidPedestrians.empty())
	{
		pedestrians_.Remove(invalidPedestrians.front());
		invalidPedestrians.pop();
	}
}

ANPCar* ATrafficController::SpawnCar(const FVector& position, const FRotator& rotation, const bool& simulate)
{
	if (templateCars_.Num() == 0)
	{
		Debug::Log("Can't spawn car: no template cars available!");
		return nullptr;
	}

	FActorSpawnParameters spawnParams;

	spawnParams.SpawnCollisionHandlingOverride
		= ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ANPCar* car = GetWorld()->SpawnActor<ANPCar>(
		templateCars_[FMath::RandRange(0, templateCars_.Num() - 1)], position, rotation, spawnParams);

#if WITH_EDITOR
	car->SetFolderPath(FName("TrafficSystems/Vehicles"));
#endif

	if (simulate)
	{
		simulatedVehicles_.Add(car);
		car->SetController(this);
		car->Simulate();
	}
	else
	{
		staticVehicles_.Add(car);
	}

	return car;
}

void ATrafficController::DeleteAllCars()
{
	for (ANPCar* car : simulatedVehicles_)
	{
		car->Destroy();
	}

	simulatedVehicles_.Empty();

	for (ANPCar* car : staticVehicles_)
	{
		car->Destroy();
	}

	staticVehicles_.Empty();
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

#if WITH_EDITOR
	pedestrian->SetFolderPath(FName("TrafficSystems/Pedestrians"));
#endif

	pedestrians_.Add(pedestrian);

	if (simulate)
	{
		pedestrian->SetController(this);
		pedestrian->Simulate();
	}

	return pedestrian;
}

void ATrafficController::DeleteAllPedestrians()
{
	for (APedestrian* pedestrian : pedestrians_)
	{
		pedestrian->Destroy();
	}

	pedestrians_.Empty();
}

void ATrafficController::BeginSimulateTraffic()
{
	// Create new cars
	DeleteAllCars();

	std::vector<int> keypointsWithMargin = roadGraph_.GetKeypointsWithMargin(4000.0f);

	// Spawn entities at "random" keypoints while avoiding spawning at the same keypoint twice. Index
	// selection is based on this blog post. 524287 is just an arbitrary big prime
	// https://lemire.me/blog/2017/09/18/visiting-all-values-in-an-array-exactly-once-in-random-order/
	const int prime = 524287;
	int randomSeed = FMath::RandRange(0, prime - 1);

	if (keypointsWithMargin.size() > 0)
	{
		for (int i = 0; i < amountOfCarsToSpawn_; i++)
		{
			// Spawn car at random keypoint
			int kpIndex = (i * prime + randomSeed) % keypointsWithMargin.size();
			const FVector position = roadGraph_.GetKeypointPosition(keypointsWithMargin[kpIndex]);

			SpawnCar(position, FRotator::ZeroRotator, true);
		}
	}
	else
	{
		Debug::Log("no road keypoints, can't spawn cars!");
	}

	// Create new pedestrians
	DeleteAllPedestrians();

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

void ATrafficController::VisualizeCollisionForEntity(ITrafficEntity* entity, const float& deltaTime) const
{
	entity->GetCollisionRectangle().Visualize(GetWorld(), deltaTime);
	entity->GetPredictedFutureCollisionRectangle().Visualize(GetWorld(), deltaTime);

	const FVector rayBegin = entity->GetCollisionRectangle().GetPosition();

	const FVector rayEnd = rayBegin + entity->GetMoveDirection() * 250.0f;

	Debug::DrawTemporaryLine(GetWorld(), rayBegin, rayEnd, FColor::Blue, deltaTime * 1.1f, 35.0f);
}

void ATrafficController::ClearLinkLines()
{
	Debug::DeletePersistentLines(GetWorld());
}

int ATrafficController::GetClosestKeypointToTemplateKeypoint()
{
	return roadGraph_.GetClosestKeypoint(templateKeypoint_->GetActorLocation());
}

void ATrafficController::RedrawLinkLines()
{
	ClearLinkLines();

	for (int i = 0; i < roadGraph_.LinkCount(); i++)
	{
		std::pair<int, int> link = roadGraph_.GetLinkKeypoints(i);

		FVector direction = (roadGraph_.GetKeypointPosition(link.second) - roadGraph_.GetKeypointPosition(link.first));
		direction.Normalize();

		Debug::DrawPersistentLine(
			GetWorld(),
			roadGraph_.GetKeypointPosition(link.first),
			roadGraph_.GetKeypointPosition(link.second),
			FColor::Green,
			15.0f);

		// directional indicator
		Debug::DrawPersistentLine(
			GetWorld(),
			roadGraph_.GetKeypointPosition(link.second) - direction * 100,
			roadGraph_.GetKeypointPosition(link.second),
			FColor::Red,
			15.f);
	}

	for (int i = 0; i < sharedUseGraph_.LinkCount(); i++)
	{
		std::pair<int, int> link = sharedUseGraph_.GetLinkKeypoints(i);

		FVector direction = (sharedUseGraph_.GetKeypointPosition(link.second) - sharedUseGraph_.GetKeypointPosition(link.first));
		direction.Normalize();

		Debug::DrawPersistentLine(
			GetWorld(),
			sharedUseGraph_.GetKeypointPosition(link.first),
			sharedUseGraph_.GetKeypointPosition(link.second),
			FColor::Green,
			15.0f);

		// directional indicator
		Debug::DrawPersistentLine(
			GetWorld(),
			sharedUseGraph_.GetKeypointPosition(link.second) - direction * 100,
			sharedUseGraph_.GetKeypointPosition(link.second),
			FColor::Red,
			15.f);
	}
}

void ATrafficController::LinkToNearest()
{
	if (!templateKeypoint_)
	{
		Debug::Log("template keypoint is null");
		return;
	}

	if (roadGraph_.KeypointCount() < 2)
	{
		Debug::Log("cannot link, not enough keypoints");
		return;
	}

	// find the keypoint closest to the keypointCreator
	const int closestKp = roadGraph_.GetClosestKeypoint(templateKeypoint_->GetActorLocation());
	roadGraph_.LinkKeypoints(roadGraph_.KeypointCount() - 1, closestKp);

	Debug::Log("linked keypoints " + FString::FromInt(roadGraph_.KeypointCount() - 1) + " and " + FString::FromInt(closestKp));
}

void ATrafficController::LinkFromNearest()
{
	if (!templateKeypoint_)
	{
		Debug::Log("template keypoint is null");
		return;
	}

	if (roadGraph_.KeypointCount() < 2)
	{
		Debug::Log("cannot link, not enough keypoints");
		return;
	}

	const int closestKp = GetClosestKeypointToTemplateKeypoint();
	roadGraph_.LinkKeypoints(closestKp, roadGraph_.KeypointCount() - 1);

	Debug::Log("linked keypoints " + FString::FromInt(closestKp) + " and " + FString::FromInt(roadGraph_.KeypointCount() - 1));
}

void ATrafficController::AddKeypointAtTemplate()
{
	if (!templateKeypoint_)
	{
		Debug::Log("template keypoint is null");
		return;
	}

	const FVector position = templateKeypoint_->GetActorLocation();

	Debug::Log(position.ToString());

	roadGraph_.AddKeypoint(position);

	if (roadGraph_.KeypointCount() >= 2)
	{
		roadGraph_.LinkKeypoints(roadGraph_.KeypointCount() - 2, roadGraph_.KeypointCount() - 1);
	}
}
