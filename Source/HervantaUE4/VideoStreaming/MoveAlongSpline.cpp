#include "MoveAlongSpline.h"
#include "../Debug.h"

AMoveAlongSpline::AMoveAlongSpline()
{
 	// Set this actor to call Tick() every frame
	PrimaryActorTick.bCanEverTick = true;

	moveSpline = CreateDefaultSubobject<USplineComponent>("spline");

	if (moveSpline)
	{
		SetRootComponent(moveSpline);
	}
}

void AMoveAlongSpline::TickInternal(float timeInterval)
{
	if (!moveSpline)
	{
		Debug::Log("spline was null");
		return;
	}

	const float DeltaTime = timeInterval == 0.0 ? 1.0 : timeInterval;
	distance += speed / DeltaTime;

	const auto new_transform = moveSpline->GetTransformAtDistanceAlongSpline(distance, ESplineCoordinateSpace::World);

	if (moveActor)
	{
		moveActor->SetActorTransform(new_transform);

		if (lockRollPitch)
		{
			const float yaw = new_transform.Rotator().Yaw;
			//const float yaw = moveActor->GetActorRotation().Yaw;
			moveActor->SetActorRotation(FRotator(0.0, yaw, 0.0));
		}
	}

	movedTransform = new_transform;
}

void AMoveAlongSpline::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (alwaysIgnoreTick)
		return;

	TickInternal(DeltaTime);
}

bool AMoveAlongSpline::ShouldTickIfViewportsOnly() const
{
	return doTick;
}

void AMoveAlongSpline::ResetDistance()
{
	Debug::Log("reset spline mover");

	doTick = false;
	distance = 0;

	const auto new_transform = moveSpline->GetTransformAtDistanceAlongSpline(distance, ESplineCoordinateSpace::World);
	movedTransform = new_transform;

	if (!moveSpline)
		return;

	moveActor->SetActorTransform(new_transform);
}

void AMoveAlongSpline::StartAnimation()
{
	Debug::Log("set tick to true");
	doTick = true;
}
