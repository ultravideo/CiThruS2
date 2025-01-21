#include "ScenarioBuilderComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Math/Vector.h"
#include "Misc/Debug.h"
#include "Kismet/GameplayStatics.h"
//#include "../CustomVehiclePawn.h"
#include "Traffic/TrafficController.h"
#include "Traffic/Intersections/IntersectionController.h"
#include "Traffic/Intersections/TrafficLightGroup.h"
#include "Traffic/Entities/Car.h"
#include "ScenariosDataHandler.h"
#include "DataStructures/ScenarioData.h"

void UScenarioBuilderComponent::BeginPlay()
{
	Super::BeginPlay();

	// Get ref to traffic controller 
	TArray<AActor*> find;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATrafficController::StaticClass(), find);
	if (find.Num() <= 0)
	{
		Debug::Log("Cannot use scenarios - no TrafficController placed");
		return;
	}
	trafficController_ = Cast<ATrafficController>(find[0]);

	// Get ref to intersections
	find.Empty();
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AIntersectionController::StaticClass(), find);
	for (int i = 0; i < find.Num(); i++)
	{
		intersections_.Add(Cast<AIntersectionController>(find[i]));
	}
}

void UScenarioBuilderComponent::RunScenario(FString ScenarioName)
{
	// Load saved data file & start/run the scenario
	KeypointGraph graph = trafficController_->GetRoadGraph();
	FScenarioData data = ScenariosDataHandler::LoadData(ScenarioName);           
	UE_LOG(LogTemp, Warning, TEXT("Loading Scenario: %s, (%s)"), *ScenarioName, *data.scenarioName) //Note: Pretty sure the data files don't contain the name anymore (data.scenarioName is empty always)
	
	// Reset NPCs
	trafficController_->DeleteAllEntities();

	// Spawn Cars
	for (int i = 0; i < data.carData.Num(); i++)
	{
		FCarScenarioData carData = data.carData[i];
		float rar = FMath::RandRange(0.0f, 1.0f);
		bool spawnCar = rar > carData.spawnRate ? false : true;
		if (!spawnCar)
		{
			continue;
		}
		
		// Note: This should set a random path starting from closest keypoint
		ACar* carInstance_ = trafficController_->SpawnCar(carData.location, carData.rotation, carData.simulate);
		if (carData.simulate)
		{
			// Check if car has a route
			if (carData.routeStart.IsZero() || carData.routeEnd.IsZero() )
			{
				//No route set, do nothing as a random path is already assigned when the car spawns
				Debug::Log("Route not set");
			}
			else 
			{
				// Create a custom path starting from the first keypoint
				int startIndex = graph.GetClosestKeypoint(carData.routeStart);
				int endIndex = graph.GetClosestKeypoint(carData.routeEnd);
				KeypointPath path = graph.FindPath(startIndex, endIndex);
				// Create a random path continuing from the end of given path
				KeypointPath pathContinuation = graph.GetRandomPathFrom(endIndex);
				pathContinuation.keypoints.RemoveAt(0);
				path.keypoints.Append(pathContinuation.keypoints);

				// Add current position to path
				TArray<FVector> customPath;
				customPath.Add(carData.location);
				path.keypoints.Insert(-8, 0);

				Debug::Log(path.keypoints.Num());
				// Apply route
				carInstance_->ApplyCustomPath(path.keypoints, customPath, 0, 0.0f);
			}
		}

		carInstance_->SetMoveSpeed(carData.speed);
		carInstance_->SetInstantSpeed(carData.speed);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *carInstance_->GetActorLocation().ToString());
		Debug::Log(carInstance_->GetPathFollower().GetPath().keypoints.Num());
	}

	// Spawn pedestrians
	for (int i = 0; i < data.pedestrianData.Num(); i++)
	{
		FPedestrianScenarioData pedData = data.pedestrianData[i];
		APedestrian* pedInstance_ = trafficController_->SpawnPedestrian(pedData.location, FRotator::ZeroRotator, true);
	}

	// Spawn player
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (pawn && data.playerData.location != FVector(0, 0, 0))
	{
		/*if (ACustomVehiclePawn* vehiclePawn = Cast<ACustomVehiclePawn>(pawn))
		{
			vehiclePawn->SetVehicleLocationRotation(data.playerData.location, data.playerData.rotation.Vector() + FVector(-90.0f, 0.0f, 0.0f));
		}
		else
		{*/
			pawn->SetActorLocationAndRotation(data.playerData.location, data.playerData.rotation, false, nullptr, ETeleportType::TeleportPhysics);
		//}	
	}

	
	// Spawn Randomized cars, avoid spawning too close to center of player
	FVector center = (!data.playerData.location.IsZero()) ? data.playerData.location : ((data.carData.Num() > 0) ? data.carData[0].location : GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation());

	for (int i = 0; i < data.randomCarAmount; i++)
	{
		for (int spawnTryLimit = 0; spawnTryLimit < 5; spawnTryLimit++)
		{
			// Get random spawn point & check that it is far enough from center
			// Note - might be better to check distance to each car instead of 1 center point
			// to prevent any randomized cars spawning too near the scenario, but that only becomes a problem
			// with very large scenarios	
			int spawnKeypoint = FMath::RandRange(0, graph.KeypointCount() - 1);
			FVector spawnLocation = graph.GetKeypointPosition(spawnKeypoint);
			if (FVector::Distance(spawnLocation, center) > data.randomCarMinDistance * 100)
			{
				ACar* carInstance_ = trafficController_->SpawnCar(spawnLocation, FRotator(), true);
				break;
			}
		}
	}

	// Set intersection / traffic light states
	for (int i = 0; i < data.intersectionData.Num(); i++)
	{
		// Error handling
		if (data.intersectionData.Num() <= i || intersections_.Num() <= i)
		{ 
			// Problem loading traffic lights, most likely intersections have been
			//  changed & are no longer compatible with an old save file
		ErrorLoadingIntersectionData:
			Debug::Log("Cannot load intersection data");
			break;
		}

		FIntersectionScenarioData idata = data.intersectionData[i];
		AIntersectionController* ic = intersections_[i];

		if (ic == nullptr)
		{
			goto ErrorLoadingIntersectionData;
		}

		for (int j = 0; j < idata.greenTimes.Num(); j++)
		{
			TArray<ATrafficLightGroup*> groups = ic->GetTrafficLightGroups();

			if (idata.greenTimes.Num() <= j || groups.Num() <= j)
			{
				goto ErrorLoadingIntersectionData;
			}

			ic->GetTrafficLightGroups()[j]->SetGreenLightDuration(idata.greenTimes[j]);
		}

		ic->SetCyclingGroup(idata.firstGreen);
		ic->ResetLights();		
	}
}

void UScenarioBuilderComponent::SaveData(FScenarioData data)
{
	UE_LOG(LogTemp, Warning, TEXT("Scenario saved w/ %d&%d cars, %d pedestrians"), data.carData.Num(), data.randomCarAmount, data.pedestrianData.Num());
	ScenariosDataHandler::SaveData(data);
}

FScenarioData UScenarioBuilderComponent::GetData(FString ScenarioName)
{
	return ScenariosDataHandler::LoadData(ScenarioName);
}

FVector UScenarioBuilderComponent::GetClosestKeypoint(FVector Location)
{
	return trafficController_->GetRoadGraph().GetKeypointPosition(trafficController_->GetRoadGraph().GetClosestKeypoint(Location));
}

TArray<FVector> UScenarioBuilderComponent::GetRoute(FVector start, FVector end)
{
	// Get the full & fastest route from position A to position B
	KeypointGraph graph = trafficController_->GetRoadGraph();
	int startIndex = graph.GetClosestKeypoint(start);
	int endIndex = graph.GetClosestKeypoint(end);
	KeypointPath path = graph.FindPath(startIndex, endIndex);

	TArray<FVector> ret;

	for (int i = 0; i < path.keypoints.Num(); i++)
	{
		ret.Add(graph.GetKeypointPosition(path.keypoints[i]));
	}

	return ret;
}

TArray<int> UScenarioBuilderComponent::GetIntersections()
{
	// Get intersection indexes, used to get number of intersections in traffic scenario editor
	TArray<int> ret;

	for (int i = 0; i < intersections_.Num(); i++)
	{ 
		ret.Add(i); 
	} 

	return ret;
}

TArray<FString> UScenarioBuilderComponent::GetIntersectionGroups(int intersectionIndex)
{
	// Get all light groups names
	TArray<FString> ret;

	for (int i = 0; i < intersections_[intersectionIndex]->GetTrafficLightGroups().Num(); i++)
	{
		ret.Add(intersections_[intersectionIndex]->GetTrafficLightGroups()[i]->GetName());
	}

	return ret;
}

TArray<float> UScenarioBuilderComponent::GetLightGroupTimes(int intersectionIndex)
{
	// Get all light group green light times of specified intersection 
	TArray<float> ret;
	TArray<ATrafficLightGroup*> groups = intersections_[intersectionIndex]->GetTrafficLightGroups();

	for (int i = 0; i < groups.Num(); i++)
	{
		ret.Add(groups[i]->GetGreenLightDuration());
	}

	return ret;
}

FVector UScenarioBuilderComponent::IntersectionMiddlePoint(int intersectionIndex)
{
	// Get the middle point position in world of an intersection
	if (intersectionIndex >= intersections_.Num())
	{
		return FVector().ZeroVector;
	}

	AIntersectionController* controller = intersections_[intersectionIndex];
	TArray<ATrafficLightGroup*> groups = controller->GetTrafficLightGroups();

	TArray<FVector> points;
	float x = 0, y = 0, z = 0;

	for (int i = 0; i < groups.Num(); i++)
	{
		TArray<AVisualTrafficLight*> lights;
		lights = groups[i]->GetVisualLights();

		for (int j = 0; j < lights.Num(); j++)
		{
			points.Add(lights[j]->GetActorLocation());
			x += points.Last().X;
			y += points.Last().Y;
			z += points.Last().Z;
		}
	}

	return FVector(x, y, z) / points.Num();
}

TArray<FString> UScenarioBuilderComponent::GetAllSaveFileNames()
{
	// Get the names of all saved traffic scenarios
	FString path = FString(FPlatformProcess::UserDir()) + TEXT("\\CiThruS2\\Scenarios\\*.sav");
	TArray<FString> files;
	IFileManager::Get().FindFiles(files, *path, true, false);

	return files;
}
