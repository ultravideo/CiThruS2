#include "TrafficStopArea.h"
#include "../MathUtility.h"
#include "ITrafficEntity.h"
#include "Pedestrian.h"
#include "../Debug.h"

#include "Components/BoxComponent.h"

#include <vector>
#include <algorithm>

ATrafficStopArea::ATrafficStopArea()
{
	UStaticMeshComponent* boxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoxMesh"));
	RootComponent = boxMesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> boxAsset(TEXT("/Game/StarterContent/Shapes/Shape_Cube.Shape_Cube"));

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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> arrowAsset(TEXT("/Game/StarterContent/Shapes/Shape_QuadPyramid.Shape_QuadPyramid"));

	if (arrowAsset.Succeeded())
	{
		arrowMesh->SetStaticMesh(arrowAsset.Object);
	}

	arrowMesh->bHiddenInGame = true;
	arrowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATrafficStopArea::BeginPlay()
{
	Super::BeginPlay();

	collisionRectangle_ = CollisionRectangle(GetActorScale() * 100.0f, GetActorLocation() + FVector::UpVector * GetActorScale().Z * 50.0f, GetActorRotation().Quaternion());
}

void ATrafficStopArea::UpdateCollisionStatusWithEntity(ITrafficEntity* entity)
{
	// Check if entity is overlapping this stop area
	if (collisionRectangle_.IsIntersecting(entity->GetCollisionRectangle()))
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
}
