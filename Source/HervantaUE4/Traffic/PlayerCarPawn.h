#pragma once

#include <vector>
#include <utility>
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PlayerCarPawn.generated.h"

// Convert the given km/h velocity to map units per second
#define KMH(velocity) (velocity / 360.0f * 10000.0f)

class ATrafficScenarios;
class ACarCautionArea;

UCLASS()
class HERVANTAUE4_API APlayerCarPawn : public APawn
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Player Car")
	AActor* frontRightRay_;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Player Car")
	AActor* frontLeftRay_;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Player Car")
	AActor* rearRightRay_;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Player Car")
	AActor* rearLeftRay_;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Player Car")
	ATrafficScenarios* trafficScenario_;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	int rpm_;

	APlayerCarPawn();

	// Called every frame
	virtual void Tick(float deltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* playerInputComponent) override;

	void ResetCar();

	void Turn(float axisValue);
	void Accelerate(float axisValue);
	void Brake(float axisValue);
	inline void ActivateHandBrake() { handBrake_ = true; };
	inline void ReleaseHandBrake() { handBrake_ = false; };
	void ShiftGearUp();
	void ShiftGearDown();

protected:
	enum Gear { Reverse, Neutral, First, Second, Third };

	const std::vector<std::pair<float, float>> gearSpeedLimits
	{ 
		{ KMH(-60.0f), 0.0f },
		{ 0.0f, 0.0f },
		{ 0.0f, KMH(60.0f) },
		{ KMH(40.0f), KMH(120.0f) },
		{ KMH(100.0f), KMH(270.0f) }
	};

	float acceleration_;
	float tireAngle_;
	float velocity_;
	float brakes_;
	bool handBrake_;
	FVector momentumVector_;
	FVector framePosition_;
	Gear gear_ = Gear::Neutral;

	void MoveCar(const float& deltaTime);
	float GetWheelHeight(const FVector& rayPos);
};
