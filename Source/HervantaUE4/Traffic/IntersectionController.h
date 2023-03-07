#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IntersectionController.generated.h"

class ATrafficLightGroup;

UCLASS()
class HERVANTAUE4_API AIntersectionController : public AActor
{
	GENERATED_BODY()
	
public:	
	AIntersectionController() { };

	void CycleFinished();

protected:
	int cyclingGroup_;

	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Intersection Setup")
	TArray<ATrafficLightGroup*> trafficLightGroups_;
};
