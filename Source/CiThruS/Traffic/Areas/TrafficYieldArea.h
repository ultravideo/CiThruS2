#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ITrafficArea.h"
#include "Traffic/CollisionRectangle.h"
#include "TrafficYieldArea.generated.h"

// An area where traffic entities need to yield and let other traffic entities go first
UCLASS()
class CITHRUS_API ATrafficYieldArea : public AActor, public ITrafficArea
{
	GENERATED_BODY()
	
public:
	ATrafficYieldArea();

	virtual void UpdateCollisionStatusWithEntity(ITrafficEntity* entity) override;
	virtual CollisionRectangle GetCollisionRectangle() const override { return collisionRectangle_; }

protected:
	CollisionRectangle collisionRectangle_;

	TSet<ITrafficEntity*> overlappingEntities_;

	virtual void BeginPlay() override;
};
