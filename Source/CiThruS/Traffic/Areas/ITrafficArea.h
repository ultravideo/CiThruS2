#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Traffic/CollisionRectangle.h"
#include "ITrafficArea.generated.h"

class ITrafficEntity;

UINTERFACE(MinimalAPI)
class UTrafficArea : public UInterface
{
	GENERATED_BODY()
};

// A common interface for static areas where traffic entities need to behave a certain way (stop, yield, etc)
class CITHRUS_API ITrafficArea
{
	GENERATED_BODY()

public:
	virtual void UpdateCollisionStatusWithEntity(ITrafficEntity* entity) = 0;
	virtual CollisionRectangle GetCollisionRectangle() const = 0;

protected:
	ITrafficArea() { };
};
