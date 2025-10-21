#include "Tram.h"
#include "Kismet/KismetMathLibrary.h"
#include "Misc/Debug.h"
#include "Traffic/Areas/TrafficStopArea.h"
#include "DrawDebugHelpers.h"

#define ENABLE_DEBUG false

ATram::ATram()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATram::BeginPlay()
{
	Super::BeginPlay();

	collisionRectangle_.SetDimensions(collisionDimensions_);
	collisionRectangle_.SetPosition(GetActorLocation());
	collisionRectangle_.SetRotation(GetActorRotation().Quaternion());
	futureCollisionRectangle_.SetDimensions(FVector(collisionDimensions_.X / carts_.Num(), collisionDimensions_.Y, collisionDimensions_.Z));

	// collisionBoundingSphereRadius is set to 1.5x the length of the diagonal of the collision rectangle: that way
	// it should always contain both the main collision rectangle and the future collision rectangle of the car
	// without being too large either
	collisionBoundingSphereRadius_ = 1.5f * sqrt(collisionDimensions_.Dot(collisionDimensions_));
}

void ATram::Destroyed()
{
	if (trafficController_ != nullptr)
		trafficController_->InvalidateTrafficEntity(this);

	Super::Destroyed();
}

void ATram::Tick(float deltaTime)
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
		if (stopArea->Active())
		{
			inActiveStopArea_ = true;
			break;
		}
	}

	Move(deltaTime);
	SetColliders();
}

void ATram::Move(const float& deltaTime)
{
	// Problem: If theres a high enough speed difference (>? 750 units/s)
	// between a car in front and a car behind.. The car behind may collide with 
	// the slower car in front

	float speedBefore_ = moveSpeed_;

	if (Stopped() || Yielding() || blocked_)
	{
		moveSpeed_ = FMath::Lerp(moveSpeed_, 0.0f, 0.99 * deltaTime);
	}
	else
	{
		if (moveSpeed_ != targetSpeed_)
		{
			if ((moveSpeed_ < 300 && moveSpeed_ > 0) || (moveSpeed_ > -300 && moveSpeed_ < 0)) // accelerate/decelerate faster at slower speeds
			{
				moveSpeed_ = FMath::Lerp(moveSpeed_, targetSpeed_, 0.97 * deltaTime);
			}
			else
			{
				moveSpeed_ = FMath::Lerp(moveSpeed_, targetSpeed_, 0.66 * deltaTime);
			}
		}
	}

	if (moveSpeed_ < 50)
	{
		// Used for animation control
		animAcceleration_ = FMath::Clamp(0, animAcceleration_ - deltaTime, animAcceleration_ + deltaTime);
		return;
	}

	// Used for animation control
	animAcceleration_ = FMath::Clamp((moveSpeed_ - speedBefore_) * deltaTime * 100.0f, animAcceleration_ - deltaTime, animAcceleration_ + deltaTime);

	pathFollower_.Advance(moveSpeed_ * deltaTime);

	FVector rearPosition = pathFollower_.GetLocation();
	FVector frontPosition = pathFollower_.GetLocation(wheelbase_);

	FRotator cartRotation = FRotationMatrix::MakeFromX(frontPosition - rearPosition).Rotator();

	//Apply movement & rotation
	SetActorLocationAndRotation((frontPosition + rearPosition) * 0.5f, cartRotation, false, nullptr, ETeleportType::TeleportPhysics);

	// Set cart locations and rotations
	for (int i = 1; i < carts_.Num(); i++)
	{
		rearPosition = pathFollower_.GetLocation(-i * (wheelbase_ + cartGap_));
		frontPosition = pathFollower_.GetLocation(-i * (wheelbase_ + cartGap_) + wheelbase_);

		/*Draw some debug lines to visualise cart edge points*/ if (ENABLE_DEBUG) { FVector a = frontPosition + FVector::UpVector * 0; FVector b = rearPosition + FVector::UpVector * 0; Debug::DrawTemporarySphere(GetWorld(), a, i % 2 == 0 ? FColor::Blue:FColor::Red, deltaTime * 2, 30); Debug::DrawTemporarySphere(GetWorld(), b, i % 2 == 0 ? FColor::Blue : FColor::Red, deltaTime * 2, 30); Debug::DrawTemporaryLine(GetWorld(), a, b, i % 2 == 1 ? FColor::Blue : FColor::Red, deltaTime * 2, 20.0f); }

		cartRotation = FRotationMatrix::MakeFromX(frontPosition - rearPosition).Rotator();
		carts_[i]->SetWorldLocationAndRotation(frontPosition, cartRotation);
	}
}

void ATram::SetColliders()
{
	FVector front = carts_[0]->GetComponentLocation();
	FVector rear = carts_.Last()->GetComponentLocation() - carts_.Last()->GetComponentRotation().Vector() * wheelbase_;

	collisionRectangle_.SetPosition((front + rear) * 0.5f);
	collisionRectangle_.SetRotation(FRotationMatrix::MakeFromX(front - rear).ToQuat());

	// Predict where this tram will be in the future
	// Note: future position affected by current moveSpeed. At higher speeds the car can then see further and start braking sooner.
	FVector futureRearAxlePosition = pathFollower_.GetLocation(wheelbase_ + moveSpeed_ * 0.01f);
	FVector futureFrontAxlePosition = pathFollower_.GetLocation(wheelbase_ * 2.0f + moveSpeed_ * 0.01f);

	// We could also calculate a future position for the Rear Axle, to get more accurate future rotation
	const FQuat futureRotation = FRotationMatrix::MakeFromX(futureFrontAxlePosition - futureRearAxlePosition).ToQuat();

	futureCollisionRectangle_.SetPosition((futureFrontAxlePosition + futureRearAxlePosition) * 0.5f + FVector::UpVector * collisionDimensions_.Z * 0.5f);
	futureCollisionRectangle_.SetRotation(futureRotation);
}

float ATram::GetAnimSteeringAngle()
{
	// Figure out the rotation of the front wheels
	FVector a = collisionRectangle_.GetRotation().Vector();
	FVector ar = collisionRectangle_.GetRotation().GetRightVector();
	FVector b = futureCollisionRectangle_.GetRotation().Vector();
	float lr = -FVector::DotProduct(a - b, ar);

	float sign = lr == 0.0f ? 0.0f : (lr < 0 ? -1.0f : 1.0f);
	float angle = FMath::Acos(FVector::DotProduct(a, b)) * 90;

	return sign * angle * 2.0f;
}

void ATram::OnFar()
{
	Far();
}

void ATram::OnNear()
{
	Near();
}

void ATram::UpdateBlockingCollisionWith(ITrafficEntity* otherEntity)
{
	// If we're already blocked, no need to check for further blocking since it wouldn't change anything
	if (blocked_)
	{
		return;
	}

	blocked_ = BlockedBy(otherEntity);
}

void ATram::UpdateBlockingCollisionWith(FVector position)
{
	// If we're already blocked, no need to check for further blocking since it wouldn't change anything
	if (blocked_)
	{
		return;
	}

	blocked_ = BlockedBy(position);
}

void ATram::ClearBlockingCollisions()
{
	blocked_ = false;
}

bool ATram::BlockedBy(ITrafficEntity* trafficEntity) const
{
	if (trafficEntity == nullptr)
	{
		return false;
	}

	const CollisionRectangle& myCollision = GetCollisionRectangle();
	const CollisionRectangle& otherCollision = trafficEntity->GetCollisionRectangle();

	// narrowFrontConeLength is the longest distance the car can see -> use as threshold
	if (FVector::DistSquared(myCollision.GetPosition(), otherCollision.GetPosition()) > collisionBoundingSphereRadius_ * collisionBoundingSphereRadius_)
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

bool ATram::BlockedBy(const FVector& point) const
{
	const FVector myPosition = GetActorLocation();

	if ((point - myPosition).SizeSquared() > collisionBoundingSphereRadius_ * collisionBoundingSphereRadius_)
	{
		return false;
	}

	return GetCollisionRectangle().IsIntersecting(point);
}

void ATram::OnEnteredStopArea(ATrafficStopArea* stopArea)
{
	overlappedStopAreas_.Add(stopArea);
}

void ATram::OnExitedStopArea(ATrafficStopArea* stopArea)
{
	overlappedStopAreas_.Remove(stopArea);
}

void ATram::Simulate(const KeypointGraph* graph)
{
	pathFollower_.Initialize(graph, this);

	useEditorTick_ = true;
	simulate_ = true;
}
