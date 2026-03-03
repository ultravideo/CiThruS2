#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Traffic/CollisionRectangle.h"
#include "Traffic/TrafficController.h"
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
	virtual bool Blocked() const = 0;

	virtual FVector GetMoveDirection() const = 0;
	virtual float GetMoveSpeed() const = 0;

	virtual CollisionRectangle GetCollisionRectangle() const = 0;
	virtual CollisionRectangle GetPredictedFutureCollisionRectangle() const = 0;

	virtual FString GetName() const = 0;

	virtual int32 GetKeypointRuleExceptions() const { return 0; }
	virtual std::pair<int32, int32> GetCurrentZone() const { return currentZone_; }

	virtual void Visualize(float duration) const = 0;

protected:
	ITrafficEntity() { }
		
	std::pair<int32, int32> currentZone_ = std::make_pair(0, 0);
	std::pair<int32, int32> previousZone_ = std::make_pair(0, 0);
		
	virtual void UpdateZone(ATrafficController* controller)
	{
		AActor* actor = Cast<AActor>(this);
		if (actor && controller)
		{
			currentZone_ = std::make_pair(static_cast<int>(actor->GetActorLocation().X / 1000.0f), static_cast<int>(actor->GetActorLocation().Z / 1000.0f));
			if (previousZone_.first != currentZone_.first || previousZone_.second != currentZone_.second)
			{
				//UE_LOG(LogTemp, Warning, TEXT("Updating zone for %s from (%d, %d) to (%d, %d)"), *GetName(), previousZone_.first, previousZone_.second, currentZone_.first, currentZone_.second);
				controller->EntityChangeZone(currentZone_, previousZone_, this);
				previousZone_ = currentZone_;
			}
			//else UE_LOG(LogTemp, Warning, TEXT("Zone for %s is still (%d, %d)"), *GetName(), currentZone_.first, currentZone_.second);
		}
		//else UE_LOG(LogTemp, Error, TEXT("ALERT: Casting of entity %s to AActor failed (or traffic controller not set)"), *GetName());
	}
};
