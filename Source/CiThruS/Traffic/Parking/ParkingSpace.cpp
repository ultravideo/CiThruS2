#include "ParkingSpace.h"
#include "ParkingController.h"
#include "Traffic/Entities/Car.h"
#include "Misc/Debug.h"

void AParkingSpace::BeginPlay()
{
	Super::BeginPlay();

	if (AParkingController* parkingController = AParkingController::Find(GetWorld()))
	{
		parkingController->RegisterParkingSpace(this);
	}
}

void AParkingSpace::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	// Must unregister so the controller never holds a reference to a streamed-out parking space
	if (AParkingController* parkingController = parkingController_.Get())
	{
		parkingController->UnregisterParkingSpace(this);
	}

	Super::EndPlay(endPlayReason);
}

void AParkingSpace::SetParkingController(AParkingController* controller)
{
	parkingController_ = controller;
}

AParkingController* AParkingSpace::GetParkingController() const
{
	return parkingController_.Get();
}

bool AParkingSpace::ParkCar(ACar* car)
{
	if (occupied_ || occupant_.IsValid() || !parkingController_.IsValid())
	{
		return false;
	}

	// This is the bare minimum amount of things that need to be done to park a car. Inheriting classes implement fancier parking and departure
	occupant_ = car;
	FinishParking();

	return true;
}

bool AParkingSpace::DepartCar()
{
	if (!occupied_ || occupant_.IsValid() || !parkingController_.IsValid())
	{
		return false;
	}

	// This is the bare minimum amount of things that need to be done to depart a car. Inheriting classes implement fancier parking and departure
	FTransform parkedTransform = GetParkedTransform();

	ACar* car = parkingController_->GetTrafficController()->SpawnCar(parkedTransform.GetLocation(), FRotator(parkedTransform.GetRotation()), true, carClass_, carVariant_);

	parkingController_->DestroyParkedInstance(visualInstanceId_);

	occupied_ = false;
	visualInstanceId_ = -1;
	carClass_ = nullptr;
	carVariant_ = -1;

	return true;
}

bool AParkingSpace::SpawnCar()
{
	if (occupied_ || occupant_.IsValid() || !parkingController_.IsValid())
	{
		return false;
	}

	visualInstanceId_ = parkingController_->CreateParkedInstance(GetParkedTransform(), carClass_, carVariant_);

	occupied_ = true;

	return true;
}

bool AParkingSpace::ClearCar()
{
	if (!occupied_ || !parkingController_.IsValid())
	{
		return false;
	}

	parkingController_->DestroyParkedInstance(visualInstanceId_);

	occupied_ = false;
	visualInstanceId_ = -1;
	carClass_ = nullptr;
	carVariant_ = -1;

	return false;
}

bool AParkingSpace::FinishParking()
{
	ACar* occupant = occupant_.Get();

	// The occupant can be destroyed while it is driving in, in which case the space stays free
	if (occupant == nullptr || !parkingController_.IsValid())
	{
		return false;
	}

	// Replace the vehicle actor with a parked vehicle instance.
	// The current vehicle should automatically start a new path and teleport somewhere else as another active vehicle. Otherwise
	// the actor should get deleted, which in turn should create another vehicle actor starting from a parking space or a spawn keypoint.

	visualInstanceId_ = parkingController_->CreateParkedInstanceForCar(occupant);

	carClass_ = occupant->GetClass();
	carVariant_ = occupant->GetVariantId();

	occupant_ = nullptr;
	occupied_ = true;

	return true;
}
