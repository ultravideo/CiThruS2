#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ITrafficArea.h"
#include "CollisionRectangle.h"
#include "TrafficYieldArea.generated.h"

UCLASS()
class HERVANTAUE4_API ATrafficYieldArea : public AActor, public ITrafficArea
{
	GENERATED_BODY()
	
public:
	ATrafficYieldArea();

	virtual void UpdateCollisionStatusWithEntity(ITrafficEntity* entity) override;
	virtual CollisionRectangle GetCollisionRectangle() const override { return collisionRectangle_; };

protected:
	CollisionRectangle collisionRectangle_;

	TSet<ITrafficEntity*> overlappingEntities_;

	virtual void BeginPlay() override;
};
