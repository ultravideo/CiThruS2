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

	ATrafficController* GetTrafficController() { return trafficController_; }

protected:
	struct InstanceData
	{
		UHierarchicalInstancedStaticMeshComponent* hism;
		FTransform localTransform;
	};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float parkingDensity_ = 1;

	TArray<AParkingSpace*> parkingSpaces_;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	ATrafficController* trafficController_;

	TMap<TTuple<UClass*, int>, TArray<InstanceData>> instanceComponents_;

	std::vector<std::vector<std::tuple<UHierarchicalInstancedStaticMeshComponent*, int>>> instances_;
	std::unordered_map<UHierarchicalInstancedStaticMeshComponent*, std::vector<int>> instanceIndices_;
	std::unordered_map<UHierarchicalInstancedStaticMeshComponent*, std::vector<int>> topIndices_;
};
