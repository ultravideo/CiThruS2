#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ITrafficArea.h"
#include "CollisionRectangle.h"
#include "TrafficStopArea.generated.h"

UCLASS()
class HERVANTAUE4_API ATrafficStopArea : public AActor, public ITrafficArea
{
	GENERATED_BODY()
	
public:	
	ATrafficStopArea();
	
	virtual void UpdateCollisionStatusWithEntity(ITrafficEntity* entity) override;
	virtual CollisionRectangle GetCollisionRectangle() const override { return collisionRectangle_; };

	inline void Activate() { active_ = true; };
	inline void Deactivate() { active_ = false; };

	inline bool Active() const { return active_; };

protected:
	bool active_;

	CollisionRectangle collisionRectangle_;

	TSet<ITrafficEntity*> overlappingEntities_;

	virtual void BeginPlay() override;
};
