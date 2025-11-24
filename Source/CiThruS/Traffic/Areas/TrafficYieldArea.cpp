#include "TrafficYieldArea.h"
#include "Traffic/Entities/ITrafficEntity.h"
#include "Misc/MathUtility.h"
#include "Misc/Debug.h"

#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"

#include <vector>
#include <algorithm>

ATrafficYieldArea::ATrafficYieldArea()
{
	UStaticMeshComponent* visualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	RootComponent = visualMesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> visualAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));

	if (visualAsset.Succeeded())
	{
		visualMesh->SetStaticMesh(visualAsset.Object);
	}

	visualMesh->bHiddenInGame = true;
	visualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

#if WITH_EDITOR
	SetIsSpatiallyLoaded(false);
#endif
}

void ATrafficYieldArea::BeginPlay()
{
	Super::BeginPlay();

	collisionRectangle_ = CollisionRectangle(GetActorScale() * 100.0f, GetActorLocation() + FVector::UpVector * GetActorScale().Z * 50.0f, GetActorRotation().Quaternion());
}

void ATrafficYieldArea::UpdateCollisionStatusWithEntity(ITrafficEntity* entity)
{
	// Check if entity is overlapping this yield area
	if (collisionRectangle_.IsIntersecting(entity->GetCollisionRectangle()))
	{
		bool actorWasAlreadyInSet = false;

		overlappingEntities_.Add(entity, &actorWasAlreadyInSet);

		// Notify the entity if it was not previously in this yield area
		if (!actorWasAlreadyInSet)
		{
			entity->OnEnteredYieldArea(this);
		}
	}
	else
	{
		// Notify the entity if it was previously in this yield area
		if (overlappingEntities_.Remove(entity) != 0)
		{
			entity->OnExitedYieldArea(this);
		}
	}
}
