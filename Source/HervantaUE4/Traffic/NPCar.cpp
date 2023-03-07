#include "NPCar.h"
#include "../MathUtility.h"
#include "TrafficStopArea.h"
#include "Pedestrian.h"
#include "../Debug.h"
#include "CollisionRectangle.h"

#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h" 
#include "Camera/PlayerCameraManager.h"

ANPCar::ANPCar()
{
 	// Set this actor to call Tick() every frame
	PrimaryActorTick.bCanEverTick = true;
}

void ANPCar::BeginPlay()
{
	Super::BeginPlay();

	collisionRectangle_.SetDimensions(collisionDimensions_);
	collisionRectangle_.SetPosition(GetActorLocation() + FVector::UpVector * collisionDimensions_.Z * 0.5f);
	collisionRectangle_.SetRotation(GetActorRotation().Quaternion());
}

bool ANPCar::CollidingWith(ITrafficEntity* trafficEntity) const
{
	if (trafficEntity == nullptr)
	{
		return false;
	}

	const CollisionRectangle& myCollision = GetCollisionRectangle();
	const CollisionRectangle& otherCollision = trafficEntity->GetCollisionRectangle();

	// narrowFrontConeLength is the longest distance the car can see -> use as threshold
	if (FVector::DistSquared(myCollision.GetPosition(), otherCollision.GetPosition()) > narrowFrontConeLength_ * narrowFrontConeLength_)
	{
		return false;
	}

	const CollisionRectangle& myFutureCollision = GetPredictedFutureCollisionRectangle();

	if (myCollision.IsIntersecting(otherCollision)
		|| myFutureCollision.IsIntersecting(otherCollision)
		|| myFutureCollision.IsIntersecting(trafficEntity->GetPredictedFutureCollisionRectangle()))
	{
		// The cars are overlapping, try to separate them. The important thing here is that one car
		// is allowed to go while the other is forced to stop so that the cars stop overlapping. This
		// method does not always work if more than 2 cars are overlapping in the same spot because
		// they might form a deadlock where A is waiting for B, B is waiting for C and C is waiting for A.
		// Handling a deadlock like that would require a more advanced system.

		const FVector aDir(GetMoveDirection());
		const FVector bDir(trafficEntity->GetMoveDirection());

		const float abDirDot = FVector::DotProduct(aDir, bDir);

		FVector aToB(otherCollision.GetPosition() - myCollision.GetPosition());
		FVector abDir(aToB.GetSafeNormal());

		const float abDot = FVector::DotProduct(aDir, abDir);

		if (myCollision.GetPosition() == otherCollision.GetPosition())
		{
			// If the cars are in the exact same position, let the car with the larger pointer value
			// go first (arbitrary order to break up the overlap but it's important that both cars
			// independently know which car should go first, pointer value works for that)
			return this > trafficEntity;
		}

		if (abDirDot >= 0.0f)
		{
			// If the cars are going in the same direction, let the car in the front go first
			const float baDot = FVector::DotProduct(bDir, -abDir);

			// This method doesn't work if the dot products are zero
			if (abDot != 0.0f && baDot != 0.0f)
			{
				const bool bInFrontOfA = abDot > 0.0f;
				const bool aInFrontOfB = baDot > 0.0f;

				// In certain scenarios both cars might be considered to be in front of the other,
				// like if the cars are facing each other. Because of this, we should only let the front
				// car go first if both cars agree that one car is in the front and the other is not.
				if (bInFrontOfA && !aInFrontOfB || !bInFrontOfA && aInFrontOfB)
				{
					return bInFrontOfA;
				}
			}
		}

		// If one car is yielding, make that one wait
		if (trafficEntity->Yielding() && !Yielding())
		{
			return false;
		}

		if (Yielding() && !trafficEntity->Yielding())
		{
			return true;
		}

		// Try to determine which car has more free space in front of it by comparing how far to
		// the side they are from each other
		const float abSidewaysDistSquared = (aToB - aToB.ProjectOnToNormal(aDir)).SizeSquared();
		const float baSidewaysDistSquared = (aToB - aToB.ProjectOnToNormal(bDir)).SizeSquared();

		if (abSidewaysDistSquared == baSidewaysDistSquared)
		{
			// If the sideways distances are the exact same, let the car with the larger pointer
			// value go first (arbitrary order to break up the overlap but it's important that both
			// cars independently know which car should go first, pointer value works for that)
			return this > trafficEntity;
		}

		// Let the car with more sideways space go first
		return abSidewaysDistSquared < baSidewaysDistSquared;
	}

	return false;
}

bool ANPCar::CollidingWith(const FVector& point) const
{
	const FVector myPosition = GetActorLocation();

	if ((point - myPosition).SizeSquared() > wideFrontConeLength_ * wideFrontConeLength_)
	{
		return false;
	}

	if (MathUtility::PointInsideCone(FVector2D(point), FVector2D(myPosition), FVector2D(moveDirection_), wideFrontConeAngle_, wideFrontConeLength_))
	{
		return true;
	}

	return false;
}

void ANPCar::NewPath(const bool& fromNearestKeypoint)
{
	if (trafficController_ == nullptr)
	{
		return;
	}

	const KeypointGraph& graph = trafficController_->GetRoadGraph();

	if (fromNearestKeypoint)
	{
		pathFollower_.SetPath(graph.GetRandomPathFrom(graph.GetClosestKeypoint(GetActorLocation())));
	}
	else
	{
		// Spawn only at the allocated spawn points
		pathFollower_.SetPath(graph.GetRandomPathFrom(graph.GetRandomSpawnPoint()));
	}

	// Set a sensible starting location and rotation according to the path
	SetActorLocation(pathFollower_.GetCurrentOrigin());

	FVector frontAxlePosition = pathFollower_.GetPointOnPath(GetActorLocation(), wheelbase_ * 0.5f);
	FVector newForward = frontAxlePosition - GetActorLocation();

	if (newForward != FVector::ZeroVector)
	{
		newForward.Normalize();

		SetActorRotation(UKismetMathLibrary::MakeRotFromXY(newForward, -FVector::CrossProduct(newForward, FVector::UpVector)));
	}

	// Remove old blocking cars and pedestrians
	blockListMutex_.Lock();
	blockingActors_.Empty();
	blockListMutex_.Unlock();
}

CollisionRectangle ANPCar::GetPredictedFutureCollisionRectangle() const
{
	// Predict where this car will be in the future
	const FVector futureRearAxlePosition = pathFollower_.GetPointOnPath(GetActorLocation() - GetActorForwardVector() * wheelbase_ * 0.5f, wheelbase_);
	const FVector futureFrontAxlePosition = pathFollower_.GetPointOnPath(futureRearAxlePosition, wheelbase_);

	FVector futureForward = futureFrontAxlePosition - futureRearAxlePosition;

	if (futureForward != FVector::ZeroVector)
	{
		futureForward.Normalize();
	}
	else
	{
		futureForward = moveDirection_;
	}

	const FQuat futureRotation = UKismetMathLibrary::MakeRotFromXY(futureForward, -FVector::CrossProduct(futureForward, FVector::UpVector)).Quaternion();
	return CollisionRectangle(collisionRectangle_.GetDimensions(),
		futureRearAxlePosition + futureForward * wheelbase_ * 0.5f + FVector::UpVector * collisionDimensions_.Z * 0.5f, futureRotation);
}

void ANPCar::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

	// Check if any overlapped stop areas are active
	inActiveStopArea_ = false;

	for (ATrafficStopArea* stopArea : overlappedStopAreas_)
	{
		if (stopArea->Active())
		{
			inActiveStopArea_ = true;
			break;
		}
	}

	Move(deltaTime);
}

void ANPCar::Move(const float& deltaTime)
{
	if (Stopped())
	{
		// No need to move
		return;
	}

	// If the current path has been fully traversed, get a new one
	if (pathFollower_.FullyTraversed())
	{
		NewPath(false);
	}

	// Move and rotate car
	FVector rearAxlePosition = GetActorLocation() - GetActorForwardVector() * wheelbase_ * 0.5f;

	moveDirection_ = pathFollower_.GetDirectionOnPathAndAdvance(rearAxlePosition, deltaTime * moveSpeed_);

	if (moveDirection_ == FVector::ZeroVector)
	{
		return;
	}

	moveDirection_.Normalize();

	rearAxlePosition += moveDirection_ * deltaTime * moveSpeed_;

	FVector frontAxlePosition = pathFollower_.GetPointOnPath(rearAxlePosition, wheelbase_);
	FVector newForward = frontAxlePosition - rearAxlePosition;

	if (newForward != FVector::ZeroVector)
	{
		newForward.Normalize();
	}
	else
	{
		newForward = moveDirection_;
	}

	SetActorLocation(rearAxlePosition + newForward * wheelbase_ * 0.5f, false);
	SetActorRotation(UKismetMathLibrary::MakeRotFromXY(newForward, -FVector::CrossProduct(newForward, FVector::UpVector)));

	collisionRectangle_.SetPosition(GetActorLocation() + FVector::UpVector * collisionDimensions_.Z * 0.5f);
	collisionRectangle_.SetRotation(GetActorRotation().Quaternion());
}

void ANPCar::AddBlockingActor(AActor* actor)
{
	blockListMutex_.Lock();

	if (blockingActors_.Contains(actor))
	{
		blockListMutex_.Unlock();
		return;
	}

	blockingActors_.Add(actor);

	blockListMutex_.Unlock();
}

void ANPCar::RemoveBlockingActor(AActor* actor)
{
	blockListMutex_.Lock();
	blockingActors_.Remove(actor);
	blockListMutex_.Unlock();
}

void ANPCar::OnEnteredStopArea(ATrafficStopArea* stopArea)
{
	overlappedStopAreas_.Add(stopArea);
}

void ANPCar::OnExitedStopArea(ATrafficStopArea* stopArea)
{
	overlappedStopAreas_.Remove(stopArea);
}

void ANPCar::OnEnteredYieldArea(ATrafficYieldArea* yieldArea)
{
	bool isAlreadyInSet = false;

	overlappedYieldAreas_.Add(yieldArea, &isAlreadyInSet);

	if (!isAlreadyInSet)
	{
		shouldYield_ = true;
	}
}

void ANPCar::OnExitedYieldArea(ATrafficYieldArea* yieldArea)
{
	overlappedYieldAreas_.Remove(yieldArea);

	if (overlappedYieldAreas_.Num() == 0)
	{
		shouldYield_ = false;
	}
}

void ANPCar::Simulate()
{
	NewPath(true);

	useEditorTick_ = true;
}
