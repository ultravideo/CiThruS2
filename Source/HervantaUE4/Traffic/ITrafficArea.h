#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CollisionRectangle.h"
#include "ITrafficArea.generated.h"

class ITrafficEntity;

UINTERFACE(MinimalAPI)
class UTrafficArea : public UInterface
{
	GENERATED_BODY()
};

class HERVANTAUE4_API ITrafficArea
{
	GENERATED_BODY()

public:
	virtual void UpdateCollisionStatusWithEntity(ITrafficEntity* entity) = 0;
	virtual CollisionRectangle GetCollisionRectangle() const = 0;

protected:
	ITrafficArea() { };
};
