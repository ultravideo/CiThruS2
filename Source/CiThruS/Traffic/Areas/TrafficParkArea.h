#pragma once

#include "CoreMinimal.h"
#include "ITrafficArea.h"
#include "TrafficParkArea.generated.h"

// Parent class for triggers that start the parking sequence. BP_ParkedCar_ParkingSequenceTrigger is the child blueprint used instead of this
UCLASS(Abstract)
class CITHRUS_API ATrafficParkArea : public AActor, public ITrafficArea
{
	GENERATED_BODY()

public:
	ATrafficParkArea() { };

	/* Trigger when vehicle enters this trigger actor */
	UFUNCTION(BlueprintImplementableEvent)
	void CarEnterParkArea(ACar* car, bool& success);

	// Used for checking overlapping with vehicle entities.
	virtual CollisionRectangle GetCollisionRectangle() const override { return collisionRectangle_; }

	virtual void UpdateCollisionStatusWithEntity(ITrafficEntity* entity) override;

protected:

	/* Rules that need to be met in order for parking sequence to trigger? Default to parking access only, i.e. 1.*/// 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Bitmask, BitmaskEnum = "/Script/CiThruS.EKeypointRules"), Category = "Park Area Setup")
	int32 parkingRules_ = 1;

	TSet<ITrafficEntity*> overlappingEntities_;
	CollisionRectangle collisionRectangle_;

	virtual void BeginPlay() override;
};
