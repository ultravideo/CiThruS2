#include "ParkingController.h"
#include "Misc/Debug.h"

#include "Traffic/Entities/Car.h"
#include "Traffic/TrafficController.h"
#include "ParkingSpace.h"
#include "Traffic/Entities/ITrafficEntity.h"

#include "Math/UnrealMathUtility.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/SCS_Node.h"
#include "Engine/InheritableComponentHandler.h"

AParkingController::AParkingController()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AParkingController::BeginPlay()
{
	Super::BeginPlay();
	
	// Get ref to traffic controller 
	TArray<AActor*> find;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATrafficController::StaticClass(), find);

	if (find.Num() <= 0)
	{
		Debug::Log("No TrafficController placed");
		return;
	}

	trafficController_ = Cast<ATrafficController>(find[0]);

	for (TSubclassOf<ACar> templateCar : trafficController_->GetTemplateCars())
	{
		TArray<FCarVisualVariant> parkedVariants = Cast<ACar>(templateCar->GetDefaultObject(true))->GetParkedVariants();

		for (FCarVisualVariant variant : parkedVariants)
		{
			TArray<InstanceData> instances;

			for (FCarVisualPart part : variant.parts)
			{
				auto instance = Cast<UHierarchicalInstancedStaticMeshComponent>(AddComponentByClass(UHierarchicalInstancedStaticMeshComponent::StaticClass(), false, FTransform::Identity, false));
				instanceIndices_[instance] = {};
				topIndices_[instance] = {};

				instance->SetStaticMesh(part.mesh);

				for (int i = 0; i < part.materials.Num(); i++)
				{
					instance->SetMaterial(i, part.materials[i]);
				}

				instances.Add({ instance, part.transform });
			}

			instanceComponents_.Add(TTuple<UClass*, int>(templateCar, variant.variantId), instances);
		}
	}

	TArray<AActor*> allParkingSpaces;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AParkingSpace::StaticClass(), allParkingSpaces);

	for (AActor* parkingSpaceActor : allParkingSpaces)
	{
		AParkingSpace* parkingSpace = Cast<AParkingSpace>(parkingSpaceActor);

		parkingSpace->SetParkingController(this);

		parkingSpaces_.Add(parkingSpace);

		if (FMath::RandRange(0.0f, 1.0f) <= parkingDensity_)
		{
			parkingSpace->SpawnCar();
		}
	}
}

bool AParkingController::DepartRandomParkedCar()
{
	if (parkingSpaces_.Num() == 0)
	{
		return false;
	}

	return parkingSpaces_[FMath::RandRange(0, parkingSpaces_.Num() - 1)]->DepartCar();
}

int AParkingController::CreateParkedInstance(FTransform transform, TSubclassOf<ACar>& carClassOut, int& carVariantOut)
{
	TArray<TTuple<UClass*, int>> instanceKeys;
	instanceComponents_.GenerateKeyArray(instanceKeys);

	if (instanceKeys.IsEmpty())
	{
		Debug::Log("No parked instances available");
		return -1;
	}

	TTuple<UClass*, int> key = instanceKeys[FMath::RandRange(0, instanceKeys.Num() - 1)];
	carClassOut = key.Get<0>();
	carVariantOut = key.Get<1>();

	TArray<InstanceData> instances = instanceComponents_[key];
	std::vector<std::tuple<UHierarchicalInstancedStaticMeshComponent*, int>> instanceIds;
	instanceIds.reserve(instances.Num());

	for (InstanceData instance : instances)
	{
		FTransform localTransform;
		FTransform::Multiply(&localTransform, &instance.localTransform, &transform);
		int instanceId = instance.hism->AddInstance(localTransform, true);
		instanceIndices_[instance.hism].push_back(instanceId);
		topIndices_[instance.hism].push_back(instanceIndices_[instance.hism].size() - 1);
		instanceIds.push_back({ instance.hism, instanceIndices_[instance.hism].size() - 1 });
	}

	instances_.push_back(instanceIds);

	return instances_.size() - 1;
}

int AParkingController::CreateParkedInstanceForCar(ACar* car)
{
	if (car == nullptr)
	{
		Debug::Log("Tried to create parked instance for nullptr");
		return -1;
	}

	auto key = car->GetClass();

	TArray<InstanceData>* instances = instanceComponents_.Find(TTuple<UClass*, int>(key, car->GetVariantId()));

	if (instances == nullptr)
	{
		Debug::Log("No parked instance found for " + key->GetName() + " variant " + FString::FromInt(car->GetVariantId()));
		return -1;
	}

	std::vector<std::tuple<UHierarchicalInstancedStaticMeshComponent*, int>> instanceIds;
	instanceIds.reserve(instances->Num());

	for (InstanceData instance : *instances)
	{
		FTransform localTransform;
		FTransform carTransform = car->GetActorTransform();
		FTransform::Multiply(&localTransform, &instance.localTransform, &carTransform);
		int instanceId = instance.hism->AddInstance(localTransform, true);
		instanceIndices_[instance.hism].push_back(instanceId);
		topIndices_[instance.hism].push_back(instanceIndices_[instance.hism].size() - 1);
		instanceIds.push_back({ instance.hism, instanceIndices_[instance.hism].size() - 1 });
	}

	instances_.push_back(instanceIds);

	return instances_.size() - 1;
}

void AParkingController::DestroyParkedInstance(int instanceId)
{
	if (instanceId < 0 || instanceId >= instances_.size())
	{
		Debug::Log("Tried to destroy invalid parked instance " + FString::FromInt(instanceId));
		return;
	}

	for (auto instance : instances_[instanceId])
	{
		std::get<0>(instance)->RemoveInstance(instanceIndices_[std::get<0>(instance)][std::get<1>(instance)]);

		instanceIndices_[std::get<0>(instance)][topIndices_[std::get<0>(instance)][topIndices_[std::get<0>(instance)].size() - 1]] = instanceIndices_[std::get<0>(instance)][std::get<1>(instance)];
		topIndices_[std::get<0>(instance)][instanceIndices_[std::get<0>(instance)][std::get<1>(instance)]] = topIndices_[std::get<0>(instance)][topIndices_[std::get<0>(instance)].size() - 1];

		instanceIndices_[std::get<0>(instance)][std::get<1>(instance)] = -1;
		topIndices_[std::get<0>(instance)].pop_back();
	}

	// TODO: The top level vector is not resized here, meaning this is a very small memory leak
	instances_[instanceId] = {};
}
