#include "Bicycle.h"
#include "Traffic/TrafficController.h"
#include "Traffic/Areas/TrafficStopArea.h"
#include "Misc/Debug.h"

#include "Math/UnrealMathUtility.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"

const float HEIGHT_CM = 180.0f;

ABicycle::ABicycle()
{
	// Set this actor to call Tick() every frame
	PrimaryActorTick.bCanEverTick = true;
}

void ABicycle::BeginPlay()
{
	Super::BeginPlay();

	collisionRectangle_.SetDimensions(collisionDimensions_);
	collisionRectangle_.SetPosition(GetActorLocation());
	collisionRectangle_.SetRotation(GetActorRotation().Quaternion());
}

void ABicycle::Destroyed()
{
	if (trafficController_ != nullptr)
	{
		trafficController_->InvalidateTrafficEntity(this);
	}

	Super::Destroyed();
}

FVector ABicycle::PreferredSpawnPositionOffset()
{
	// This is needed because Unreal Engine characters have their origin at their center instead
	// of at their feet so they're spawned inside the ground if they're spawned exactly at the keypoints
	return FVector::UpVector * HEIGHT_CM * 0.5f;
}

void ABicycle::Simulate(const KeypointGraph* graph)
{

	moveSpeed_ = baseSpeed_ + FMath::RandRange(-randomSpeed_, randomSpeed_);

	pathFollower_.Initialize(graph, this, false);
	useEditorTick_ = true;
	simulate_ = true;
}

void ABicycle::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

	if (!simulate_)
	{
		return;
	}


	// Check if any overlapped stop areas are active
	inActiveStopArea_ = false;

	for (ATrafficStopArea* stopArea : overlappedStopAreas_)
	{
		// Also check that the stop area is pointing in the direction we're trying to go in.
		// sqrt(3)/2 as the dot product means the difference between the pedestrian's direction and the stop area direction is 60 degrees max
		if (stopArea->Active() && FVector2D::DotProduct(FVector2D(stopArea->GetActorForwardVector()), FVector2D(moveDirection_)) > UE_HALF_SQRT_3)
		{
			inActiveStopArea_ = true;
			moveSpeed_ = baseSpeed_ + FMath::RandRange(-randomSpeed_, randomSpeed_); // Reroll speed when stopped
			break;
		}
	}

	if ( Stopped() || Blocked() ) 
	{
		// No need to move
		return;
	}
	else
	{
		// Slowly increase speed over time
		moveSpeed_ = FMath::Clamp(moveSpeed_ + (moveSpeed_ * 0.00001f), baseSpeed_ - randomSpeed_, baseSpeed_ + randomSpeed_);

		pathFollower_.Advance(moveSpeed_ * deltaTime);
		currentTurnAmount_ = pathFollower_.GetTurnAmount();


		//UE_LOG(LogTemp, Warning, TEXT("moveSpeed_: %0.4f, deltaTime: %0.4f, add: %0.4f, mul: %0.4f"), moveSpeed_, deltaTime, moveSpeed_ + deltaTime, moveSpeed_ * deltaTime);

		FVector tangent;
		FVector rearAxlePosition = pathFollower_.GetLocation(tangent);
		FVector middlePosition = rearAxlePosition + tangent * wheelbase_ * 0.5f;


		//Apply movement & rotation

		moveDirection_ = tangent;
		//int currentPoint = GetPathFollower().GetCurrentLocalPoint();


		SetActorLocationAndRotation(middlePosition, moveDirection_.ToOrientationQuat(), false, nullptr, ETeleportType::TeleportPhysics);

		moveDirection_ = GetActorForwardVector();

		// Update collision
		/*
		collisionRectangle_.SetPosition(GetActorLocation());
		collisionRectangle_.SetRotation(GetActorRotation().Quaternion());
		*/
		SetColliders();
	}
}

void ABicycle::SetColliders()
{
	// Update collision
	collisionRectangle_.SetPosition(GetActorLocation() + FVector::UpVector * collisionDimensions_.Z * 0.5f);
	collisionRectangle_.SetRotation(GetActorRotation().Quaternion());

	// Predict where this car will be in the future
	// The car needs to see further when going faster, in order to react in time
	float futureScale = FMath::Clamp(moveSpeed_ * 0.004 - 1, 1.0f, INFINITY);

	FVector tangent;
	FVector futureRearAxlePosition = pathFollower_.GetLocation((wheelbase_ + collisionDimensions_.X * futureScale) * 0.5, tangent);

	futureCollisionRectangle_.SetDimensions(FVector(collisionDimensions_.X * futureScale, collisionDimensions_.Y, collisionDimensions_.Z));
	futureCollisionRectangle_.SetPosition(futureRearAxlePosition + tangent * wheelbase_ * 0.5f + FVector::UpVector * collisionDimensions_.Z * 0.5f);
	futureCollisionRectangle_.SetRotation(tangent.ToOrientationQuat());
}


void ABicycle::UpdateBlockingCollisionWith(ITrafficEntity* otherEntity)
{
	// If we're already blocked, no need to check for further blocking since it wouldn't change anything
	if (blocked_)
	{
		return;
	}

	blocked_ = BlockedBy(otherEntity);
	if (blocked_) {
		blockingEntity_ = otherEntity->GetName();
		if (!otherEntity->Stopped() && !otherEntity->Blocked())
		{
			// someone is going slow in front, reduce speed!
			moveSpeed_ = FMath::Clamp(moveSpeed_ - (moveSpeed_ * 0.01f), baseSpeed_ - randomSpeed_, baseSpeed_ + randomSpeed_);
		}
	}
}

void ABicycle::UpdateBlockingCollisionWith(FVector position)
{
	// If we're already blocked, no need to check for further blocking since it wouldn't change anything
	if (blocked_)
	{
		return;
	}

	blocked_ = BlockedBy(position);
	if (blocked_) blockingPosition_ = position;
}

void ABicycle::UpdatePawnCollision(CollisionRectangle pawnCollision)
{
	if (futureCollisionRectangle_.IsIntersecting(pawnCollision))
	{
		blockedByFuturePawn_ = true;
	}
}

void ABicycle::ClearBlockingCollisions()
{
	blocked_ = false;
	blockedByFuturePawn_ = false;
	blockingEntity_ = "";
	blockingPosition_ = FVector::ZeroVector;
}

bool ABicycle::BlockedBy(ITrafficEntity* trafficEntity) const
{
	if (trafficEntity == nullptr)
	{
		return false;
	}

	const CollisionRectangle& myCollision = GetCollisionRectangle();
	const CollisionRectangle& myFutureCollision = GetPredictedFutureCollisionRectangle();
	const CollisionRectangle& otherCollision = trafficEntity->GetCollisionRectangle();
	const CollisionRectangle& otherFutureCollision = trafficEntity->GetPredictedFutureCollisionRectangle();

	FVector dimensionSum = myCollision.GetDimensions() + otherCollision.GetDimensions() + myFutureCollision.GetDimensions() + otherFutureCollision.GetDimensions();

	if (FVector::DistSquared(myCollision.GetPosition(), otherCollision.GetPosition()) > FVector::DotProduct(dimensionSum, dimensionSum) * 0.25f)
	{
		// If the distance between the collision rectangles is larger than half of the sum of their diagonals, they are too far away to ever intersect no matter their rotation
		return false;
	}

	if (myCollision.IsIntersecting(otherCollision)
		|| myFutureCollision.IsIntersecting(otherCollision)
		|| myFutureCollision.IsIntersecting(otherFutureCollision))
	{
		// The cars are overlapping, try to separate them. The important thing here is that one car
		// is allowed to go while the other is forced to stop so that the cars stop overlapping. This
		// method does not always work if more than 2 cars are overlapping in the same spot because
		// they might form a deadlock where A is waiting for B, B is waiting for C and C is waiting for A.
		// Handling a deadlock like that would require a more advanced system

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
				if ((bInFrontOfA && !aInFrontOfB) || (!bInFrontOfA && aInFrontOfB))
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

bool ABicycle::BlockedBy(const FVector& point) const
{
	const FVector myPosition = GetActorLocation();
	const FVector myDimensions = GetCollisionRectangle().GetDimensions();

	if (FVector::DistSquared(point, myPosition) > FVector::DotProduct(myDimensions, myDimensions) * 0.25f)
	{
		// If the distance between the point and the collision rectangle is larger than half of the rectangle's diagonal, they are too far away to ever intersect no matter their rotation
		return false;
	}

	return GetCollisionRectangle().IsIntersecting(FVector2D(point.X, point.Y));
}


void ABicycle::OnEnteredStopArea(ATrafficStopArea* stopArea)
{
	overlappedStopAreas_.insert(stopArea);
}

void ABicycle::OnExitedStopArea(ATrafficStopArea* stopArea)
{
	overlappedStopAreas_.erase(stopArea);
}

void ABicycle::OnEnteredYieldArea(ATrafficYieldArea* yieldArea)
{
	bool isAlreadyInSet = false;

	overlappedYieldAreas_.Add(yieldArea, &isAlreadyInSet);

	if (!isAlreadyInSet)
	{
		shouldYield_ = true;
	}
}

void ABicycle::OnExitedYieldArea(ATrafficYieldArea* yieldArea)
{
	overlappedYieldAreas_.Remove(yieldArea);

	if (overlappedYieldAreas_.Num() == 0)
	{
		shouldYield_ = false;
	}
}

void ABicycle::OnFar()
{
	Far();
}

void ABicycle::OnNear()
{
	Near();
}

/*
CollisionRectangle ABicycle::GetPredictedFutureCollisionRectangle() const
{
	// Predict where this pedestrian will be in the future
	return CollisionRectangle(collisionRectangle_.GetDimensions(),
		GetActorLocation() + moveDirection_ * (collisionDimensions_.X + collisionDimensions_.Y) * 0.5f, GetActorRotation().Quaternion());
}
*/
