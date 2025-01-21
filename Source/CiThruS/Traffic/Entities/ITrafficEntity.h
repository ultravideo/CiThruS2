#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Traffic/CollisionRectangle.h"
#include "ITrafficEntity.generated.h"

class ATrafficStopArea;
class ATrafficYieldArea;
class ATrafficController;

UINTERFACE(MinimalAPI)
class UTrafficEntity : public UInterface
{
	GENERATED_BODY()
};

// A common interface for all simulated traffic entities (cars, pedestrians, trams etc.)
class CITHRUS_API ITrafficEntity
{
	GENERATED_BODY()

public:
	virtual void SetController(ATrafficController* controller) = 0;

	virtual void OnEnteredStopArea(ATrafficStopArea* stopArea) = 0;
	virtual void OnExitedStopArea(ATrafficStopArea* stopArea) = 0;

	virtual void OnEnteredFutureStopArea(ATrafficStopArea* stopArea) { }
	virtual void OnExitedFutureStopArea(ATrafficStopArea* stopArea) { }

	virtual void OnEnteredYieldArea(ATrafficYieldArea* stopArea) = 0;
	virtual void OnExitedYieldArea(ATrafficYieldArea* stopArea) = 0;

	virtual void OnFar() { }
	virtual void OnNear() { }

	virtual void UpdateBlockingCollisionWith(ITrafficEntity* otherEntity) = 0;
	virtual void UpdateBlockingCollisionWith(FVector position) = 0;
	virtual void UpdatePawnCollision(CollisionRectangle pawnCollision) { }

	virtual void ClearBlockingCollisions() = 0;

	virtual bool Yielding() const = 0;
	virtual bool Stopped() const = 0;

	virtual FVector GetMoveDirection() const = 0;

	virtual CollisionRectangle GetCollisionRectangle() const = 0;
	virtual CollisionRectangle GetPredictedFutureCollisionRectangle() const = 0;

	inline virtual int32 GetKeypointRuleExceptions() const { return 0; }

protected:
	ITrafficEntity() { }
};
