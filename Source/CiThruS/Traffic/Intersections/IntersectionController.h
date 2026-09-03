#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IntersectionController.generated.h"

class ATrafficLightGroup;
class AVisualTrafficLight;
class ATrafficStopArea;

// Controls the traffic lights in a single intersection by cycling them periodically
UCLASS()
class CITHRUS_API AIntersectionController : public AActor
{
	GENERATED_BODY()
	
public:	
	AIntersectionController() { }

	void CycleFinished();

	TArray<ATrafficLightGroup*> GetTrafficLightGroups() { return trafficLightGroups_; }
	
	void SetCyclingGroup(int i) { cyclingGroup_ = i; }

	void ResetLights();

	bool WillBeGreenNext(AVisualTrafficLight* trafficLight) const;
	bool WasGreenPreviously(AVisualTrafficLight* trafficLight) const;

	bool WillDeactivateNext(ATrafficStopArea* stopArea) const;
	bool WasDeactivatedPreviously(ATrafficStopArea* stopArea) const;

protected:
	int cyclingGroup_;

	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Intersection Setup")
	TArray<ATrafficLightGroup*> trafficLightGroups_;
};
