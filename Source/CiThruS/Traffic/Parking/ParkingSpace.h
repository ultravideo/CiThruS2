#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ParkingSpace.generated.h"

// Minimum amount of keypoint that a car has traversed, before it can attempt to park into a parking space.
#define MIN_TRAVERSED_DISTANCE_BEFORE_PARKING 15

class ACar;
class AParkingController;

// Abstract base class for all parking spaces.
// A car can drive into a parking space, turning it into a cheap static parked car which is not
// simulated. Cars can also depart from parking spaces, in which case the static parked car is
// swapped for a simulated car again
UCLASS()
class CITHRUS_API AParkingSpace : public AActor
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	virtual bool ParkCar(ACar* car);

	UFUNCTION(BlueprintCallable)
	virtual bool DepartCar();

	UFUNCTION(BlueprintCallable)
	virtual bool SpawnCar();

	UFUNCTION(BlueprintCallable)
	virtual bool ClearCar();

	virtual bool FinishParking();

	void SetParkingController(AParkingController* controller);
	AParkingController* GetParkingController() const;
	bool HasParkedCar() const { return occupied_ && !occupant_.IsValid() && visualInstanceId_ >= 0; }
	FTransform GetPublishTransform() { return GetParkedTransform(); }
	int GetVisualInstanceId() const { return visualInstanceId_; }

protected:
	AParkingSpace() : visualInstanceId_(-1) { }

	// Parking spaces are streamed in and out at runtime, so each one registers itself with the
	// parking controller here instead of relying only on the controller's one-off startup sweep
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	UFUNCTION(BlueprintNativeEvent)
	FTransform GetParkedTransform();
	FTransform GetParkedTransform_Implementation() { return GetActorTransform(); }

	// These reference actors whose lifetimes are independent of this one: the controller outlives
	// parking spaces, and an occupant car can be destroyed while it is driving in. Weak pointers
	// go null when the actor is destroyed instead of dangling into freed memory
	TWeakObjectPtr<AParkingController> parkingController_;
	TWeakObjectPtr<ACar> occupant_;

	bool occupied_;
	int visualInstanceId_;
	TSubclassOf<ACar> carClass_;
	int carVariant_;
};
