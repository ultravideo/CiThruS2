#include "PlayerVehicleController.h"
//#include "ChaosVehicleWheel.h"
//#include "ChaosWheeledVehicleMovementComponent.h"
#include "Misc/Debug.h"

void APlayerVehicleController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	//movementComponent_ = InPawn->GetComponentByClass<UChaosWheeledVehicleMovementComponent>();

	spawn_.first = InPawn->GetActorLocation();
	spawn_.second = InPawn->GetActorRotation().Quaternion();
}

void APlayerVehicleController::OnUnPossess()
{
	Super::OnUnPossess();
	//movementComponent_ = nullptr;
}

void APlayerVehicleController::BeginPlay()
{
	Super::BeginPlay();
}
/*
void APlayerVehicleController::ChangeWheelSetup(TArray<TSubclassOf<UChaosVehicleWheel>> wheelClasses)
{
	if (!IsValid(movementComponent_))
	{
		Debug::Log("Can't find movement component ???");
		return;
	}

	if (movementComponent_->WheelSetups.Num() < wheelClasses.Num())
	{
		Debug::Log("not enough wheels given");
		return;
	}

	for (int i = 0; i < wheelClasses.Num(); i++)
	{
		if (!IsValid(wheelClasses[i]))
		{
			continue;
		}
		// Create a new FChaosWheelSetup
		FChaosWheelSetup newWheelSetup = FChaosWheelSetup();
		newWheelSetup.WheelClass = wheelClasses[i];
		newWheelSetup.BoneName = movementComponent_->WheelSetups[i].BoneName;

		// Replace the wheel setup default
		movementComponent_->WheelSetups[i] = newWheelSetup;
	}

	// Reset the vehicle to re-initialize wheels
	movementComponent_->ResetVehicle();

	Debug::Log("Vehicle Setup changed");
}*/

void APlayerVehicleController::SetVehicleLocationRotation(FVector location, FVector rotation, bool relative)
{
	APawn* pawn = GetPawn();
	if (pawn == nullptr)
	{
		return;
	}

	if (relative)
	{
		location = location + pawn->GetActorLocation();
		rotation = rotation + pawn->GetActorRotation().Vector();
	}

	if (USkeletalMeshComponent* mesh = pawn->FindComponentByClass<USkeletalMeshComponent>())
	{	
		mesh->SetAllPhysicsPosition(location);
		// 1. Acts weird when there is movement
		//mesh->SetAllPhysicsRotation(rotation.ToOrientationQuat());
	}
	// 2. This is needed to avoid weird movement after teleporting
	// movementComponent_->StopAllMovementImmediately();
}

void APlayerVehicleController::ResetVehicle(bool toSpawn)
{
	if (toSpawn)
	{
		SetVehicleLocationRotation(spawn_.first, spawn_.second.ToRotationVector(), false);
	}
	else
	{
		SetVehicleLocationRotation(FVector(100,100,100), FVector(), true);
	}
}
