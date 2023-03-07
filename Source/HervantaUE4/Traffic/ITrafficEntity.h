#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CollisionRectangle.h"
#include "ITrafficEntity.generated.h"

class ATrafficStopArea;
class ATrafficYieldArea;
class ATrafficController;

UINTERFACE(MinimalAPI)
class UTrafficEntity : public UInterface
{
	GENERATED_BODY()
};

class HERVANTAUE4_API ITrafficEntity
{
	GENERATED_BODY()

public:
	virtual void SetController(ATrafficController* controller) = 0;

	virtual void OnEnteredStopArea(ATrafficStopArea* stopArea) = 0;
	virtual void OnExitedStopArea(ATrafficStopArea* stopArea) = 0;

	virtual void OnEnteredYieldArea(ATrafficYieldArea* stopArea) = 0;
	virtual void OnExitedYieldArea(ATrafficYieldArea* stopArea) = 0;

	virtual bool Yielding() const = 0;
	virtual bool Stopped() const = 0;

	virtual FVector GetMoveDirection() const = 0;
	virtual CollisionRectangle GetCollisionRectangle() const = 0;
	virtual CollisionRectangle GetPredictedFutureCollisionRectangle() const = 0;

protected:
	ITrafficEntity() { };
};
