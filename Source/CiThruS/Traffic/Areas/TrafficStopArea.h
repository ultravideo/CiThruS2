#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ITrafficArea.h"
#include "Traffic/CollisionRectangle.h"
#include "TrafficStopArea.generated.h"

// An area where traffic entities need to stop. Can be activated and deactivated to let them pass conditionally
UCLASS()
class CITHRUS_API ATrafficStopArea : public AActor, public ITrafficArea
{
	GENERATED_BODY()
	
public:	
	ATrafficStopArea();
	
	virtual void UpdateCollisionStatusWithEntity(ITrafficEntity* entity) override;
	virtual CollisionRectangle GetCollisionRectangle() const override { return collisionRectangle_; }

	inline void Activate() { active_ = true; }
	inline void Deactivate() { active_ = false; }

	inline bool Active() const { return active_; }

protected:
	bool active_;

	CollisionRectangle collisionRectangle_;

	TSet<ITrafficEntity*> overlappingEntities_;
	TSet<ITrafficEntity*> futureOverlappingEntities_;

	virtual void BeginPlay() override;
};
