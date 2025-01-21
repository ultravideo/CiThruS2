#include "ParkingSpace.h"
#include "ParkingController.h"
#include "Traffic/Entities/Car.h"
#include "Misc/Debug.h"

bool AParkingSpace::ParkCar(ACar* car)
{
	if (occupied_ || occupant_ != nullptr || parkingController_ == nullptr)
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
	if (!occupied_ || occupant_ != nullptr || parkingController_ == nullptr)
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
	if (occupied_ || occupant_ != nullptr || parkingController_ == nullptr)
	{
		return false;
	}

	visualInstanceId_ = parkingController_->CreateParkedInstance(GetParkedTransform(), carClass_, carVariant_);

	occupied_ = true;

	return true;
}

bool AParkingSpace::ClearCar()
{
	if (!occupied_ || parkingController_ == nullptr)
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
	if (occupant_ == nullptr || parkingController_ == nullptr)
	{
		return false;
	}

	// Replace the vehicle actor with a parked vehicle instance.
	// The current vehicle should automatically start a new path and teleport somewhere else as another active vehicle. Otherwise 
	// the actor should get deleted, which in turn should create another vehicle actor starting from a parking space or a spawn keypoint.

	visualInstanceId_ = parkingController_->CreateParkedInstanceForCar(occupant_);

	carClass_ = occupant_->GetClass();
	carVariant_ = occupant_->GetVariantId();

	occupant_ = nullptr;
	occupied_ = true;

	return true;
}
