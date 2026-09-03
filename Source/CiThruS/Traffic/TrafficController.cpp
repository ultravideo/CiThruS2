#include "TrafficController.h"
#include "Entities/Car.h"
#include "Entities/Pedestrian.h"
#include "Entities/Bicycle.h"
#include "Areas/TrafficStopArea.h"
#include "Areas/TrafficParkArea.h"
#include "Areas/TrafficYieldArea.h"
#include "EngineUtils.h"
#include "Misc/MathUtility.h"
#include "Misc/Debug.h"
#include "Entities/Tram/Tram.h"
#include "Parking/ParkingController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Runtime/Core/Public/Async/ParallelFor.h"
#include "LodController.h"

#if WITH_EDITOR
#include "GameFramework/SpectatorPawn.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#endif // WITH_EDITOR

#include <list>

ATrafficController::ATrafficController()
	: parkingController_(nullptr),
	lodController_(nullptr),
	massDeletionInProgress_(false),
	visualizeCollisions_(false)
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

	// Load keypoints
	roadGraph_.LoadFromFile(TCHAR_TO_UTF8(*(FPaths::ProjectDir() + "/DataFiles/roadGraph.data")));
	roadGraph_.AlignWithWorldGround(GetWorld());
	
	sharedUseGraph_.LoadFromFile(TCHAR_TO_UTF8(*(FPaths::ProjectDir() + "/DataFiles/sharedUseGraph.data")));
	sharedUseGraph_.AlignWithWorldGround(GetWorld());

	tramwayGraph_.LoadFromFile(TCHAR_TO_UTF8(*(FPaths::ProjectDir() + "/DataFiles/tramwayTrackGraph.data")));
	tramwayGraph_.AlignWithWorldGround(GetWorld());

	bicycleGraph_.LoadFromFile(TCHAR_TO_UTF8(*(FPaths::ProjectDir() + "/DataFiles/bicycleGraph.data")));
	bicycleGraph_.AlignWithWorldGround(GetWorld());

	// Fetch traffic areas
	trafficAreas_.Empty();

	for (TActorIterator<AActor> it = TActorIterator<AActor>(GetWorld()); it; it.operator++())
	{
		if (ITrafficArea* trafficArea = Cast <ITrafficArea>(*it))
		{
			trafficAreas_.Add(trafficArea);
		}
	}

	// Apply extra rules to car keypoints from VehicleRuleZones, incase they aren't applied manually
	roadGraph_.ApplyZoneRules(this);

	if (simulateParking_)
	{
		// Get ref to parking controller
		TArray<AActor*> find;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AParkingController::StaticClass(), find);

		if (find.Num() <= 0)
		{
			Debug::Log("No ParkingController placed");
		}
		else
		{
			parkingController_ = Cast<AParkingController>(find[0]);
		}
	}
	else
	{
		parkingController_ = nullptr;
	}

	if (simulateTraffic_)
	{
		BeginSimulateTraffic();
	}
}

void ATrafficController::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	Super::EndPlay(endPlayReason);

	delete lodController_;
	lodController_ = nullptr;

	delete rng_;
	rng_ = nullptr;
}

void ATrafficController::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

	CheckEntityCollisions();

	if (lodController_ != nullptr)
	{
		lodController_->UpdateAllLods();
	}

	if (visualizeCollisions_)
	{
		for (ITrafficArea* trafficArea : trafficAreas_)
		{
			trafficArea->Visualize(deltaTime);
		}

		for (ITrafficEntity* entity : simulatedEntities_)
		{
			entity->Visualize(deltaTime);
		}

		for (ITrafficEntity* entity : staticEntities_)
		{
			entity->Visualize(deltaTime);
		}
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

/* 	// Collect current pawns in the scene
	std::list<APawn*> pawns;
	for (TActorIterator<APawn> it = TActorIterator<APawn>(GetWorld()); it; it.operator++())
	{
		if (*it == nullptr)
		{
			continue;
		}

		pawns.push_back(*it);
	} */

	// Check against traffic entities
	for (int i = 0; i < simulatedEntities_.size(); i++)
	{
		if (ACar* car = Cast<ACar>(simulatedEntities_[i]))
		{
			if (MathUtility::PointInsideRectangle(FVector2D(car->GetActorLocation()), av, bv, cv, dv))
			{
				colliding.Add(car);
			}
		}
	}

	// Check against player vehicles
	APawn* playerPawn = GetWorld()->GetFirstPlayerController()->GetPawn();
	if (playerPawn && MathUtility::PointInsideRectangle(FVector2D(playerPawn->GetActorLocation()), av, bv, cv, dv))
	{
		colliding.Add(playerPawn);
	}

/* 	// Check against pawns
	for (APawn* pawn : pawns)
	{
		if (MathUtility::PointInsideRectangle(FVector2D(pawn->GetActorLocation()), av, bv, cv, dv))
		{
			colliding.Add(pawn);
		}
	} */

	return colliding;
}

ACar* ATrafficController::SpawnCar()
{
	int spawnPoint = roadGraph_.GetRandomSpawnPoint(*rng_);

	return SpawnCar(roadGraph_.GetKeypointPosition(spawnPoint), roadGraph_.GetKeypointRotation(spawnPoint), true);
}

ACar* ATrafficController::SpawnCar(const FVector& position, const FRotator& rotation, const bool& simulate)
{
	if (templateCars_.Num() == 0)
	{
		Debug::Log("Can't spawn car: no template cars available!");
		return nullptr;
	}

	// The car variant is NOT the same as this index, it's a variant WITHIN the template car class like a version of it with a specific color
	int templateCarIndex = rng_->RandRange(0, templateCars_.Num() - 1);

	return SpawnCar(position, rotation, simulate, templateCars_[templateCarIndex], -1);
}

ACar* ATrafficController::SpawnCar(const FVector& position, const FRotator& rotation, const bool& simulate, const TSubclassOf<ACar>& carClass, const int& carVariant)
{
	FActorSpawnParameters spawnParams{};

	spawnParams.SpawnCollisionHandlingOverride
		= ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

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
		simulatedEntities_.push_back(car);
		car->SetController(this);

		// This must be called before car->Simulate because Simulate may immediately destroy the car
		// to respawn it, in which case lodController would get a pointer to an already destroyed car
		if (lodController_ != nullptr)
		{
			lodController_->AddEntity(car);
		}

		car->Simulate(&roadGraph_, rng_->RandRange(std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
	}
	else
	{
		staticEntities_.push_back(car);
	}

	return car;
}

APedestrian* ATrafficController::SpawnPedestrian()
{
	int spawnPoint = sharedUseGraph_.GetRandomSpawnPoint(*rng_);

	return SpawnPedestrian(sharedUseGraph_.GetKeypointPosition(spawnPoint), sharedUseGraph_.GetKeypointRotation(spawnPoint), true);
}

APedestrian* ATrafficController::SpawnPedestrian(const FVector& position, const FRotator& rotation, const bool& simulate)
{
	if (templatePedestrians_.Num() == 0)
	{
		Debug::Log("Can't spawn pedestrian: no template pedestrians available!");
		return nullptr;
	}

	// The pedestrian variant is NOT the same as this index, it's a variant WITHIN the template pedestrian class like a version of it with specific clothing
	int templatePedestrianIndex = rng_->RandRange(0, templatePedestrians_.Num() - 1);

	return SpawnPedestrian(position, rotation, simulate, templatePedestrians_[templatePedestrianIndex], -1);
}

APedestrian* ATrafficController::SpawnPedestrian(const FVector& position, const FRotator& rotation, const bool& simulate, const TSubclassOf<APedestrian>& pedestrianClass, const int& pedestrianVariant)
{
	FActorSpawnParameters spawnParams{};

	spawnParams.SpawnCollisionHandlingOverride
		= ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APedestrian* pedestrian = GetWorld()->SpawnActor<APedestrian>(pedestrianClass,
		position + APedestrian::PreferredSpawnPositionOffset(), rotation, spawnParams);

	if (!pedestrian)
	{
		Debug::Log("Pedestrian spawn failed");
		return nullptr;
	}

	if (pedestrianVariant >= 0)
	{
		// TODO
		//pedestrian->SetVariantId(pedestrianVariant);
	}

#if WITH_EDITOR
	pedestrian->SetFolderPath(FName("TrafficSystems/Pedestrians"));
#endif

	if (simulate)
	{
		simulatedEntities_.push_back(pedestrian);
		pedestrian->SetController(this);

		if (lodController_ != nullptr)
		{
			lodController_->AddEntity(pedestrian);
		}

		pedestrian->Simulate(&sharedUseGraph_, rng_->RandRange(std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
	}
	else
	{
		staticEntities_.push_back(pedestrian);
	}

	return pedestrian;
}

ABicycle* ATrafficController::SpawnBicycle()
{
	int spawnPoint = bicycleGraph_.GetRandomSpawnPoint(*rng_);

	return SpawnBicycle(bicycleGraph_.GetKeypointPosition(spawnPoint), bicycleGraph_.GetKeypointRotation(spawnPoint), true);
}

ABicycle* ATrafficController::SpawnBicycle(const FVector& position, const FRotator& rotation, const bool& simulate)
{
	if (templateBicycles_.Num() == 0)
	{
		Debug::Log("Can't spawn bicycle: no template bicycles available!");
		return nullptr;
	}

	// The bicycle variant is NOT the same as this index, it's a variant WITHIN the template bicycle class like a version of it with a specific color
	int templateBicycleIndex = rng_->RandRange(0, templateBicycles_.Num() - 1);

	return SpawnBicycle(position, rotation, simulate, templateBicycles_[templateBicycleIndex], -1);
}

ABicycle* ATrafficController::SpawnBicycle(const FVector& position, const FRotator& rotation, const bool& simulate, const TSubclassOf<ABicycle>& bicycleClass, const int& bicycleVariant)
{
	FActorSpawnParameters spawnParams{};

	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABicycle* bicycle = GetWorld()->SpawnActor<ABicycle>(bicycleClass,
		position /* + ABicycle::PreferredSpawnPositionOffset()*/, rotation, spawnParams);
	//UE_LOG(LogTemp, Log, TEXT("Spawning bicycle at: %s"), *position.ToString());

	if (!bicycle)
	{
		Debug::Log("Bicycle spawn failed");
		return nullptr;
	}

	if (bicycleVariant >= 0)
	{
		// TODO
		// bicycle->SetVariantId(bicycleVariant);
	}

#if WITH_EDITOR
	bicycle->SetFolderPath(FName("TrafficSystems/Bicycles"));
#endif
	
	if (simulate)
	{
		simulatedEntities_.push_back(bicycle);
		bicycle->SetController(this);

		if (lodController_ != nullptr)
		{
			lodController_->AddEntity(bicycle);
		}

		bicycle->Simulate(&bicycleGraph_, rng_->RandRange(std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
	}
	else
	{
		staticEntities_.push_back(bicycle);
	}

	return bicycle;
}

ATram* ATrafficController::SpawnTram()
{
	return nullptr;
}

ATram* ATrafficController::SpawnTram(const FVector& position, const FRotator& rotation, const bool& simulate)
{
	if (templateTrams_.Num() == 0)
	{
		Debug::Log("Can't spawn tram: no template trams available!");
		return nullptr;
	}

	// The tram variant is NOT the same as this index, it's a variant WITHIN the template tram class like a version of it with a specific color
	int templateTramIndex = rng_->RandRange(0, templateTrams_.Num() - 1);

	return SpawnTram(position, rotation, simulate, templateTrams_[templateTramIndex], -1);
}

ATram* ATrafficController::SpawnTram(const FVector& position, const FRotator& rotation, const bool& simulate, const TSubclassOf<ATram>& tramClass, const int& tramVariant)
{
	FActorSpawnParameters spawnParams{};

	spawnParams.SpawnCollisionHandlingOverride
		= ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATram* tram = GetWorld()->SpawnActor<ATram>(tramClass, position, rotation, spawnParams);

	if (!tram)
	{
		Debug::Log("Tram spawn failed");
		return nullptr;
	}

	if (tramVariant >= 0)
	{
		// TODO
		// tram->SetVariantId(tramVariant);
	}

#if WITH_EDITOR
	tram->SetFolderPath(FName("TrafficSystems/Trams"));
#endif
	
	if (simulate)
	{
		simulatedEntities_.push_back(tram);
		tram->SetController(this);

		if (lodController_ != nullptr)
		{
			lodController_->AddEntity(tram);
		}

		tram->Simulate(&tramwayGraph_, rng_->RandRange(std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
	}
	else
	{
		staticEntities_.push_back(tram);
	}

	return tram;
}

void ATrafficController::InvalidateTrafficEntity(ITrafficEntity* entity)
{
	if (entity != nullptr && !massDeletionInProgress_)
	{
		// Remove entity from zones
		EntityZoneDelete(entity->GetCurrentZone(), entity);

		auto it = std::find_if(simulatedEntities_.begin(), simulatedEntities_.end(), [entity](ITrafficEntity* wrapper) { return wrapper == entity; });
		
		if (it != simulatedEntities_.end())
		{
			if (lodController_ != nullptr)
			{
				lodController_->RemoveEntity(entity);
			}

			simulatedEntities_.erase(it);
		}
	}
}

void ATrafficController::BeginSimulateTraffic()
{
	DeleteAllEntities();

	if (parkingController_) parkingController_->EndSimulateTraffic();

	if (lodController_ != nullptr)
	{
		delete lodController_;
	}

	// Create this here in case the LOD distance is changed during simulation
	if (lowDetailEntitiesOutsideCamera_)
	{
		lodController_ = new LodController(this, distanceFromCameraToEnableLowDetail_);
	}

	if (rng_ != nullptr)
	{
		delete rng_;
	}
	
	if (trafficSeedZeroForRandom_ != 0)
	{
		rng_ = new FRandomStream(trafficSeedZeroForRandom_);
	}
	else
	{
		auto now = std::chrono::system_clock::now();

		rng_ = new FRandomStream(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 0xFFFFFFFF);
	}

	// Must be initialized before any cars are spawned in case they decide to spawn from parking spaces.
	if (parkingController_ != nullptr)
	{
		parkingController_->BeginSimulateTraffic(rng_);
	}

	SpawnCars();
	SpawnTrams();
	SpawnPedestrians();
	SpawnBicycles();

	UE_LOG(LogTemp, Log, TEXT("Spawned %d cars, %d trams, %d pedestrians and %d bicycles"), amountOfCarsToSpawn_, amountOfTramsToSpawn_, amountOfPedestriansToSpawn_, amountOfBicyclesToSpawn_);
}

void ATrafficController::SpawnCars()
{
	// Somewhat close margin based on the amount of cars, to prevent repeating keypoints, and having as much space as possible for each car 
	float kpMargin = 10000000 * FMath::Pow(amountOfCarsToSpawn_, -1.4) + 300;
	kpMargin = FMath::Clamp(kpMargin, 0, 4000);

	std::vector<int> keypointsWithMargin = roadGraph_.GetKeypointsWithMargin(kpMargin, *rng_);
	// Remove intersections from spawn location list, to prevent cars getting stuck in those
	// TODO: a better way to do this would be to mark the keypoints in the file as "never spawn here" instead of using these hardcoded areas
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
	int randomSeed = rng_->RandRange(0, prime - 1);

	if (keypointsWithMargin.size() <= 0)
	{
		Debug::Log("no road keypoints, can't spawn cars!");
		return;
	}

	int spawnNumber = 0; // Keep a separate counter to avoid spawning twice with the same one

	if (spawnEmergencyVehicle_ && emergencyVehicleActor_)
	{
		ACar* defaultEmergencyVehicleObject = emergencyVehicleActor_.GetDefaultObject();

		int32 ruleExceptions = defaultEmergencyVehicleObject->GetKeypointRuleExceptions();
		// TODO
		int variant = -1;

		int fs = 0;

		while (fs < 50)
		{
			int kpIndex = (spawnNumber++ * prime + randomSeed) % keypointsWithMargin.size();

			if (roadGraph_.CompareRules(kpIndex, ruleExceptions))
			{
				SpawnCar(roadGraph_.GetKeypointPosition(keypointsWithMargin[0]), roadGraph_.GetKeypointRotation(keypointsWithMargin[0]), true, emergencyVehicleActor_, variant);
				break;
			}

			fs++;
		}
	}

	if (templateCars_.Num() == 0)
	{
		Debug::Log("no template cars, can't spawn cars!");
		return;
	}

	for (int i = 0; i < amountOfCarsToSpawn_; i++)
	{
		// Spawn car at random keypoint, Enforce spawning by keypoint rules
		TSubclassOf<ACar> templateCar = templateCars_[rng_->RandRange(0, templateCars_.Num() - 1)];
		ACar* defaultCarObject = templateCar.GetDefaultObject();

		// Adding a null check here to avoid crashing if templateCars_ has an empty entry.
		if (!templateCar)
		{
			Debug::Log("Null car in template list");
			continue;
		}

		int32 ruleExceptions = defaultCarObject->GetKeypointRuleExceptions();
		// TODO
		int variant = -1;

		int fs = 0;

		while (fs < 50)
		{
			int kpIndex = (spawnNumber++ * prime + randomSeed) % keypointsWithMargin.size();

			if (roadGraph_.CompareRules(kpIndex, ruleExceptions))
			{
				SpawnCar(roadGraph_.GetKeypointPosition(keypointsWithMargin[kpIndex]), roadGraph_.GetKeypointRotation(keypointsWithMargin[kpIndex]), true, templateCar, variant);
				break;
			}

			fs++;
		}
	}
}

void ATrafficController::SpawnTrams()
{
	if (tramwayGraph_.KeypointCount() <= 0)
	{
		Debug::Log("No tramway keypoints, can't spawn trams!");
		return;
	}

	if (templateTrams_.Num() == 0)
	{
		Debug::Log("no template trams, can't spawn cars!");
		return;
	}

	if (amountOfTramsToSpawn_ == 0)
	{
		return;
	}

	// Spawn the trams evenly across the track
	int offset = FMath::Floor(rng_->RandRange(0, tramwayGraph_.KeypointCount() - 1) / amountOfTramsToSpawn_);

	for (int i = 0; i < amountOfTramsToSpawn_; i++)
	{
		int spawnIndex = FMath::Floor(tramwayGraph_.KeypointCount() * (float)i / (float)amountOfTramsToSpawn_) + offset;

		if (spawnIndex >= tramwayGraph_.KeypointCount())
		{
			spawnIndex -= tramwayGraph_.KeypointCount();
		}

		TSubclassOf<ATram> templateTram = templateTrams_[rng_->RandRange(0, templateTrams_.Num() - 1)];
		ATram* defaultTramObject = templateTram.GetDefaultObject();

		// Adding a null check here to avoid crashing if templateTrams_ has an empty entry.
		if (!templateTram)
		{
			Debug::Log("Null tram in template list");
			continue;
		}

		// TODO
		int variant = -1;

		SpawnTram(tramwayGraph_.GetKeypointPosition(spawnIndex), tramwayGraph_.GetKeypointRotation(spawnIndex), true, templateTram, variant);
	}
}

void ATrafficController::SpawnPedestrians()
{
	if (sharedUseGraph_.KeypointCount() <= 0)
	{
		Debug::Log("no shared use keypoints, can't spawn pedestrians!");
		return;
	}

	if (templatePedestrians_.Num() == 0)
	{
		Debug::Log("no template pedestrians, can't spawn pedestrians!");
		return;
	}

	// Spawn entities at "random" keypoints while avoiding spawning at the same keypoint twice. Index
	// selection is based on this blog post. 524287 is just an arbitrary big prime
	// https://lemire.me/blog/2017/09/18/visiting-all-values-in-an-array-exactly-once-in-random-order/
	const int prime = 524287;
	int randomSeed = rng_->RandRange(0, prime - 1);

	for (int i = 0; i < amountOfPedestriansToSpawn_; i++)
	{
		// Spawn pedestrian at random keypoint
		int kpIndex = (i * prime + randomSeed) % sharedUseGraph_.KeypointCount();
		const FVector position = sharedUseGraph_.GetKeypointPosition(kpIndex);

		TSubclassOf<APedestrian> templatePedestrian = templatePedestrians_[rng_->RandRange(0, templatePedestrians_.Num() - 1)];
		APedestrian* defaultPedestrianObject = templatePedestrian.GetDefaultObject();

		// Adding a null check here to avoid crashing if templatePedestrians_ has an empty entry.
		if (!templatePedestrian)
		{
			Debug::Log("Null pedestrian in template list");
			continue;
		}

		// TODO
		int variant = -1;

		SpawnPedestrian(position, FRotator::ZeroRotator, true, templatePedestrian, variant);
	}
}

void ATrafficController::SpawnBicycles()
{
	if (bicycleGraph_.KeypointCount() <= 0)
	{
		Debug::Log("no bicycle keypoints, can't spawn bicycles!");
		return;
	}

	if (templateBicycles_.Num() == 0)
	{
		Debug::Log("no template bicycles, can't spawn bicycles!");
		return;
	}

	// Spawn entities at "random" keypoints while avoiding spawning at the same keypoint twice. Index
	// selection is based on this blog post. 524287 is just an arbitrary big prime
	// https://lemire.me/blog/2017/09/18/visiting-all-values-in-an-array-exactly-once-in-random-order/
	const int prime = 524287;
	int randomSeed = rng_->RandRange(0, prime - 1);

	for (int i = 0; i < amountOfBicyclesToSpawn_; i++)
	{
		int kpIndex = (i * prime + randomSeed) % bicycleGraph_.KeypointCount();
		const FVector position = bicycleGraph_.GetKeypointPosition(kpIndex);
		const FRotator rotation = bicycleGraph_.GetKeypointRotation(kpIndex);

		TSubclassOf<ABicycle> templateBicycle = templateBicycles_[rng_->RandRange(0, templateBicycles_.Num() - 1)];
		ABicycle* defaultBicycleObject = templateBicycle.GetDefaultObject();

		// Adding a null check here to avoid crashing if templateBicycles_ has an empty entry.
		if (!templateBicycle)
		{
			Debug::Log("Null bicycle in template list");
			continue;
		}

		// TODO
		int variant = -1;

		SpawnBicycle(position, rotation, true, templateBicycle, variant);
	}
}

template<class T>
void ATrafficController::DeleteAllEntitiesOfType()
{
	massDeletionInProgress_ = true;

	// Remove and destroy all entities that can be cast into the given type
	auto predicate = [](ITrafficEntity* entity)
	{
		T* castEntity = Cast<T>(entity);

		if (castEntity != nullptr)
		{
			// If you get errors complaining about this spot it's because the
			// type you gave to the template doesn't have a Destroy() function (i.e. doesn't inherit from AActor)
			castEntity->Destroy();

			return true;
		}

		return false;
	};

	for (auto it = std::find_if(simulatedEntities_.begin(), simulatedEntities_.end(), predicate);
		it != simulatedEntities_.end();
		it = std::find_if(it, simulatedEntities_.end(), predicate))
	{
		if (lodController_ != nullptr)
		{
			lodController_->RemoveEntity(*it);
		}

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
	VisualizeGraph(&bicycleGraph_);
}

void ATrafficController::ToggleViewFrustrum()
{
	if (lodController_ != nullptr)
	{
		lodController_->SetVisualizeViewFrustrum(!lodController_->GetVisualizeViewFrustrum());
	}
}

void ATrafficController::SetFixedSimulationFrameRate(int frameRate)
{
	if (!GEngine)
	{
		return;
	}

	if (frameRate > 0)
	{
		GEngine->bUseFixedFrameRate = true;
		GEngine->FixedFrameRate = frameRate;
		GEngine->SetMaxFPS(frameRate);
	}
	else
	{
		GEngine->bUseFixedFrameRate = false;
		GEngine->SetMaxFPS(0);
	}
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
