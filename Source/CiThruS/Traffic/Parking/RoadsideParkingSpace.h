#pragma once

#include "ParkingSpace.h"
#include "RoadsideParkingSpace.generated.h"

// Parking space along a road. The parking and departing sequences are different than with parking spaces at parking lots
// Part of the implementation is handled in blueprints, which is not ideal...
UCLASS()
class CITHRUS_API ARoadsideParkingSpace : public AParkingSpace
{
	GENERATED_BODY()

public:
	ARoadsideParkingSpace() { };

	virtual bool ParkCar(ACar* car) override;

	virtual bool DepartCar() override;

	UFUNCTION(BlueprintCallable)
	void SetDepartingPoints(FVector pointA, FVector pointB) { departingPoints_[0] = pointA; departingPoints_[1] = pointB; }

	UFUNCTION(BlueprintCallable)
	void SetArrivingPoint(FVector point) { arrivingPoint_ = point; }

protected:
	FVector departingPoints_[2];
	FVector arrivingPoint_;
};
