#pragma once

#include "GameFramework/PlayerController.h"

#include "PlayerVehicleController.generated.h"

//class UChaosWheeledVehicleMovementComponent;

// Controller for the player's car
UCLASS()
class CITHRUS_API APlayerVehicleController : public APlayerController
{
	GENERATED_BODY()

public:
	APlayerVehicleController() {};

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	//void ChangeWheelSetup(TArray<TSubclassOf<UChaosVehicleWheel> > wheelClasses);

	void SetVehicleLocationRotation(FVector location, FVector rotation, bool relative);

	UFUNCTION(BlueprintCallable)
	void ResetVehicle(bool toSpawn);

private:
	//UChaosWheeledVehicleMovementComponent* movementComponent_;
	std::pair<FVector, FQuat> spawn_; // Loc & rot of player spawn.
};
