#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisualTrafficLight.h"

#include "TrafficLightGroup.generated.h"

class ATrafficStopArea;
class AIntersectionController;

UCLASS()
class HERVANTAUE4_API ATrafficLightGroup : public AActor
{
	GENERATED_BODY()
	
public:
	ATrafficLightGroup();

	virtual void Tick(float deltaTime) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	UFUNCTION(BlueprintCallable)
	ETrafficLightState GetLightState() const { return lightState_; };

	void ConnectToIntersectionController(AIntersectionController* intersection);
	void Cycle();

protected:
	ETrafficLightState lightState_;
	float timeRemainingInCurrentState_;

	AIntersectionController* intersection_;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic Light Setup")
	TArray<AVisualTrafficLight*> visualLights_;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic Light Setup")
	TArray<ATrafficStopArea*> stopAreas_;

	float yellowLightDuration_ = 2.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic Light Setup")
	float greenLightDuration_ = 12.0f;

	virtual void BeginPlay() override;

	void SetLightState(const ETrafficLightState& newState);
};
