#include "TrafficParkArea.h"
#include "Traffic/Entities/ITrafficEntity.h"
#include "Traffic/Entities/Car.h"

void ATrafficParkArea::BeginPlay()
{
	Super::BeginPlay();

	collisionRectangle_ = CollisionRectangle(GetActorScale() * 300.0f, GetActorLocation() + FVector::UpVector * GetActorScale().Z * 50.0f, GetActorRotation().Quaternion());
}

void ATrafficParkArea::UpdateCollisionStatusWithEntity(ITrafficEntity* entity)
{
	// Check if entity is a car, no need to check further for other types
	if (ACar* car = Cast<ACar>(entity))
	{
		// Check if car is overlapping
		if (collisionRectangle_.IsIntersecting(entity->GetCollisionRectangle()))
		{
			bool actorWasAlreadyInSet = false;
			overlappingEntities_.Add(entity, &actorWasAlreadyInSet);

			// If this is the first time && car complies with rules, start the parking sequence
			if (!actorWasAlreadyInSet)
			{
				if ((parkingRules_ & car->GetKeypointRuleExceptions()) == parkingRules_)
				{
					bool success;
					CarEnterParkArea(car, success);
				}
			}
		}
		else
		{
			//Try to remove entity if it was previously in the set
			overlappingEntities_.Remove(entity);
		}
	}
}
