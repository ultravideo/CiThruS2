#pragma once

#include "ParkingSpace.h"
#include "ParkingLotParkingSpace.generated.h"

// Parking space at a parking lot. The parking and departing sequences are different than with roadside parking spaces
// Part of this is handled in blueprints, which is not ideal...
UCLASS()
class CITHRUS_API AParkingLotParkingSpace : public AParkingSpace
{
	GENERATED_BODY()

public:
	AParkingLotParkingSpace() { }

	virtual bool ParkCar(ACar* car) override;

	virtual bool DepartCar() override;

	UFUNCTION(BlueprintCallable)
	void SetDepartingPoint(FVector point) { departingPoint_ = point; }

	UFUNCTION(BlueprintCallable)
	void SetArrivingPoint(FVector point) { arrivingPoint_ = point; }

protected:
	FVector departingPoint_;
	FVector arrivingPoint_;
};
