#include "ParkingController.h"
#include "Misc/Debug.h"

#include "Traffic/Entities/Car.h"
#include "Traffic/TrafficController.h"
#include "ParkingSpace.h"
#include "Traffic/Entities/ITrafficEntity.h"
#include "Video/SegmentationController.h"

#include "Math/UnrealMathUtility.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/SCS_Node.h"
#include "Engine/InheritableComponentHandler.h"

TWeakObjectPtr<AParkingController> AParkingController::cachedInstance_;

AParkingController::AParkingController()
{
	PrimaryActorTick.bCanEverTick = false;
}

AParkingController* AParkingController::Find(const UWorld* world)
{
	if (world == nullptr)
	{
		return nullptr;
	}

	// The cache is keyed on the world so that it can't leak across PIE sessions
	AParkingController* cached = cachedInstance_.Get();

	if (cached != nullptr && cached->GetWorld() == world)
	{
		return cached;
	}

	TArray<AActor*> found;
	UGameplayStatics::GetAllActorsOfClass(const_cast<UWorld*>(world), AParkingController::StaticClass(), found);

	cached = found.Num() > 0 ? Cast<AParkingController>(found[0]) : nullptr;
	cachedInstance_ = cached;

	return cached;
}

void AParkingController::RegisterParkingSpace(AParkingSpace* parkingSpace)
{
	if (!IsValid(parkingSpace))
	{
		return;
	}

	parkingSpace->SetParkingController(this);
	parkingSpaces_.AddUnique(parkingSpace);
}

void AParkingController::UnregisterParkingSpace(AParkingSpace* parkingSpace)
{
	// Order is irrelevant here, so the O(1) swap removal is fine
	parkingSpaces_.RemoveSingleSwap(parkingSpace);
}

void AParkingController::BeginPlay()
{
	Super::BeginPlay();
	
	Initialize();
}

void AParkingController::Initialize()
{
	if (trafficController_ != nullptr)
	{
		return;
	}

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
		if (templateCar == nullptr)
		{
			Debug::Log("Template car is null, skipping");
			continue;
		}

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

				// Image segmentation tracking for parked cars
				if (segmentationTrackingParent_ == nullptr)
				{
					USegmentationController::RegisterObject(instance, ESegmentationClass::Car);
					segmentationTrackingParent_ = instance;
				}
				else
				{
					USegmentationController::RegisterSubobject(instance, segmentationTrackingParent_);
				}

				instances.Add({ instance, part.transform });
			}

			instanceComponents_.Add(TTuple<UClass*, int>(templateCar, variant.variantId), instances);
		}
	}

	TArray<AActor*> allParkingSpaces;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AParkingSpace::StaticClass(), allParkingSpaces);

	// Picks up everything already loaded; spaces streamed in later register themselves in BeginPlay
	for (AActor* parkingSpaceActor : allParkingSpaces)
	{
		RegisterParkingSpace(Cast<AParkingSpace>(parkingSpaceActor));
	}
}

void AParkingController::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	Super::EndPlay(endPlayReason);

	// Reset image segmentation tracking
	for (std::pair<UHierarchicalInstancedStaticMeshComponent* const, std::vector<int>> const& instance : instanceIndices_)
	{
		USegmentationController::UnregisterObject(instance.first);
	}

	segmentationTrackingParent_ = nullptr;

	if (cachedInstance_.Get() == this)
	{
		cachedInstance_ = nullptr;
	}
}

bool AParkingController::DepartRandomParkedCar()
{
	if (parkingSpaces_.Num() == 0)
	{
		return false;
	}

	AParkingSpace* parkingSpace = parkingSpaces_[rng_->RandRange(0, parkingSpaces_.Num() - 1)].Get();

	return parkingSpace != nullptr && parkingSpace->DepartCar();
}

bool AParkingController::HismInstanceBelongsToParkingSpace(const UHierarchicalInstancedStaticMeshComponent* hism, int hismInstance, const AParkingSpace* parkingSpace) const
{
	for (const auto instance : instances_[parkingSpace->GetVisualInstanceId()])
	{
		if (std::get<0>(instance) != hism)
		{
			continue;
		}

		return instanceIndices_.find(std::get<0>(instance))->second[std::get<1>(instance)] == hismInstance;
	}

	return false;
}

void AParkingController::BeginSimulateTraffic(FRandomStream* rng)
{
	Initialize();

	rng_ = rng;

	for (const TWeakObjectPtr<AParkingSpace>& parkingSpaceWeak : parkingSpaces_)
	{
		// Draw before the validity check so the random stream stays reproducible
		const bool spawnCar = rng_->FRandRange(0.0f, 1.0f) <= parkingDensity_;
		AParkingSpace* parkingSpace = parkingSpaceWeak.Get();

		if (spawnCar && parkingSpace != nullptr)
		{
			parkingSpace->SpawnCar();
		}
	}
}

void AParkingController::EndSimulateTraffic()
{
	for (const TWeakObjectPtr<AParkingSpace>& parkingSpaceWeak : parkingSpaces_)
	{
		if (AParkingSpace* parkingSpace = parkingSpaceWeak.Get())
		{
			parkingSpace->ClearCar();
		}
	}

	rng_ = nullptr;
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

	TTuple<UClass*, int> key = instanceKeys[rng_->RandRange(0, instanceKeys.Num() - 1)];
	carClassOut = key.Get<0>();
	carVariantOut = key.Get<1>();

	TArray<InstanceData> instances = instanceComponents_[key];
	std::vector<std::tuple<UHierarchicalInstancedStaticMeshComponent*, int>> instanceIds;
	instanceIds.reserve(instances.Num());

	for (InstanceData instance : instances)
	{
		FTransform localTransform;
		FTransform::Multiply(&localTransform, &instance.localTransform, &transform);
		// This uses AddInstances instead of AddInstance because the latter forgets to update some cache stuff and sometimes triggers an ensure statement
		int instanceId = instance.hism->AddInstances({ localTransform }, true, true, true)[0];
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
		// This uses AddInstances instead of AddInstance because the latter forgets to update some cache stuff and sometimes triggers an ensure statement
		int instanceId = instance.hism->AddInstances({ localTransform }, true, true, true)[0];
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
