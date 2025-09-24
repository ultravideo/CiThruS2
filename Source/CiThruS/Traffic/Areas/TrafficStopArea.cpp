#include "TrafficStopArea.h"
#include "Misc/MathUtility.h"
#include "Traffic/Entities/ITrafficEntity.h"
#include "Misc/Debug.h"

#include "Components/BoxComponent.h"

#include <vector>
#include <algorithm>

ATrafficStopArea::ATrafficStopArea()
{
#if WITH_EDITOR
	UStaticMeshComponent* boxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoxMesh"));
	RootComponent = boxMesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> boxAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));

	if (boxAsset.Succeeded())
	{
		boxMesh->SetStaticMesh(boxAsset.Object);
	}

	boxMesh->bHiddenInGame = true;
	boxMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UStaticMeshComponent* arrowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowMesh"));
	arrowMesh->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0.0f, -90.0f, 0.0f)));
	arrowMesh->SetRelativeLocation(FVector(50.0f, 0.0f, 50.0f));
	arrowMesh->SetupAttachment(RootComponent);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> arrowAsset(TEXT("/Engine/BasicShapes/Cone.Cone"));

	if (arrowAsset.Succeeded())
	{
		arrowMesh->SetStaticMesh(arrowAsset.Object);
	}

	arrowMesh->bHiddenInGame = true;
	arrowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SetIsSpatiallyLoaded(false);
#endif
}

void ATrafficStopArea::BeginPlay()
{
	Super::BeginPlay();

	collisionRectangle_ = CollisionRectangle(GetActorScale() * 100.0f, GetActorLocation() + FVector::UpVector * GetActorScale().Z * 50.0f, GetActorRotation().Quaternion());
}

void ATrafficStopArea::UpdateCollisionStatusWithEntity(ITrafficEntity* entity)
{
	bool isIntersecting = collisionRectangle_.IsIntersecting(entity->GetCollisionRectangle());
	bool futureIntersecting = collisionRectangle_.IsIntersecting(entity->GetPredictedFutureCollisionRectangle());

	// Check if entity is overlapping this stop area
	if (isIntersecting)
	{
		bool actorWasAlreadyInSet = false;

		overlappingEntities_.Add(entity, &actorWasAlreadyInSet);

		// Notify the entity if it was not previously in this stop area
		if (!actorWasAlreadyInSet)
		{
			entity->OnEnteredStopArea(this);
		}
	}
	else
	{
		// Notify the entity if it was previously in this stop area
		if (overlappingEntities_.Remove(entity) != 0)
		{
			entity->OnExitedStopArea(this);
		}
	}
	// Check if entity will be overlapping this in the near future
	if (futureIntersecting)
	{
		bool actorWasAlreadyInSet = false;

		futureOverlappingEntities_.Add(entity, &actorWasAlreadyInSet);
		
		if (!actorWasAlreadyInSet)
		{
			entity->OnEnteredFutureStopArea(this);
		}
	}
	else
	{
		if (futureOverlappingEntities_.Remove(entity) != 0)
		{
			entity->OnExitedFutureStopArea(this);
		}
	}
}
