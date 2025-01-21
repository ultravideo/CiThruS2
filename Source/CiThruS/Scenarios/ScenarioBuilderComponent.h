#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ScenarioBuilderComponent.generated.h"

class ScenariosDataHandler;
class ATrafficController;
class AIntersectionController;

// Contains all helper functions for Traffic Scenario blueprints
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CITHRUS_API UScenarioBuilderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UScenarioBuilderComponent() { }

	UFUNCTION(BlueprintCallable)
	void RunScenario(FString ScenarioName);

	UFUNCTION(BlueprintCallable)
	void SaveData(FScenarioData data);

	UFUNCTION(BlueprintCallable)
	FScenarioData GetData(FString ScenarioName);

	UFUNCTION(BlueprintCallable)
	FVector GetClosestKeypoint(FVector Location);

	UFUNCTION(BlueprintCallable)
	TArray<FVector> GetRoute(FVector start, FVector end);

	UFUNCTION(BlueprintCallable)
	TArray<int> GetIntersections();

	UFUNCTION(BlueprintCallable)
	TArray<FString> GetIntersectionGroups(int intersectionIndex);

	UFUNCTION(BlueprintCallable)
	TArray<float> GetLightGroupTimes(int intersectionIndex);

	UFUNCTION(BlueprintCallable)
	FVector IntersectionMiddlePoint(int intersectionIndex);

	UFUNCTION(BlueprintCallable)
	TArray<FString> GetAllSaveFileNames();
		
protected:
	virtual void BeginPlay() override;

	ATrafficController* trafficController_;
	TArray<AIntersectionController*> intersections_;
};
