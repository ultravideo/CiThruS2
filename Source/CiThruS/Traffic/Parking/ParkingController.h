#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Containers/Map.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Traffic/Areas/ITrafficArea.h"

#include <unordered_map>

#include "ParkingController.generated.h"

class ACar;
class AParkingSpace;
class ITrafficEntity;
class ATrafficController;

// Spawns all static parked cars. There should be only one of these in the environment at a time
UCLASS()
class CITHRUS_API AParkingController : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AParkingController();

	int CreateParkedInstance(FTransform transform, TSubclassOf<ACar>& carClassOut, int& carVariantOut);
	int CreateParkedInstanceForCar(ACar* car);

	void DestroyParkedInstance(int instanceId);

	bool DepartRandomParkedCar();

	bool HismInstanceBelongsToParkingSpace(const UHierarchicalInstancedStaticMeshComponent* hism, int hismInstance, const AParkingSpace* parkingSpace) const;

	void BeginSimulateTraffic(FRandomStream* rng);
	void EndSimulateTraffic();

	ATrafficController* GetTrafficController() { return trafficController_; }
	const TArray<TWeakObjectPtr<AParkingSpace>>& GetParkingSpaces() const { return parkingSpaces_; }

	// Parking spaces call these themselves as they are streamed in and out
	void RegisterParkingSpace(AParkingSpace* parkingSpace);
	void UnregisterParkingSpace(AParkingSpace* parkingSpace);

	// There should only ever be one parking controller, so the lookup is cached after the first search
	static AParkingController* Find(const UWorld* world);

protected:
	struct InstanceData
	{
		UHierarchicalInstancedStaticMeshComponent* hism;
		FTransform localTransform;
	};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float parkingDensity_ = 1;

	// Weak: parking spaces are destroyed and recreated by World Partition streaming, so these
	// must neither keep them alive nor dangle into freed memory once they are gone
	TArray<TWeakObjectPtr<AParkingSpace>> parkingSpaces_;

	static TWeakObjectPtr<AParkingController> cachedInstance_;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	void Initialize();

	ATrafficController* trafficController_;
	FRandomStream* rng_;

	TMap<TTuple<UClass*, int>, TArray<InstanceData>> instanceComponents_;

	UHierarchicalInstancedStaticMeshComponent* segmentationTrackingParent_ = nullptr;

	std::vector<std::vector<std::tuple<UHierarchicalInstancedStaticMeshComponent*, int>>> instances_;
	std::unordered_map<UHierarchicalInstancedStaticMeshComponent*, std::vector<int>> instanceIndices_;
	std::unordered_map<UHierarchicalInstancedStaticMeshComponent*, std::vector<int>> topIndices_;
};
