#include "Car.h"
#include "Misc/MathUtility.h"
#include "Traffic/Areas/TrafficStopArea.h"
#include "Pedestrian.h"
#include "Misc/Debug.h"
#include "Traffic/CollisionRectangle.h"
#include "Traffic/Paths/CurveFactory.h"

#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h" 
#include "Camera/PlayerCameraManager.h"

ACar::ACar()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.SetTickFunctionEnable(false); 	// Set to true in Simulate()
}

void ACar::BeginPlay()
{
	Super::BeginPlay();

	// Set collider dimensions
	collisionRectangle_.SetDimensions(collisionDimensions_);
	SetColliders();
}	

void ACar::Destroyed()
{
	if (trafficController_ != nullptr)
	{
		trafficController_->InvalidateTrafficEntity(this);
	}

	Super::Destroyed();
}

void ACar::Tick(float deltaTime)
{
	Super::Tick(deltaTime);
	frameCounter_++;

	if (!simulate_)
	{
		return;
	}

	// If set to waitForUnobstructedDepart, do nothing until the path is clear of other entities, see WaitForUnobstructedDepart()
	if (waitForUnobstructedDepart)
	{
		if (blocked_ || blockedByFuturePawn_) // Check if car is blocked, do nothing if it is
		{
			return;
		}
		else if (frameCounter_ >= 2) // Some stuff gets initialized on the second frame i think, so only allow to continue after that
		{
			// Stop waiting & set colliders back to normal
			waitForUnobstructedDepart = false;
		}

		// Other cases just consider the car blocked
		return;
	}

	// Check if any overlapped stop areas are active
	inActiveStopArea_ = false;
	inActiveFutureStopArea_ = false;

	for (ATrafficStopArea* stopArea : overlappedStopAreas_)
	{
		if (stopArea->Active())
		{
			inActiveStopArea_ = true;
			break;
		}
	}

	for (ATrafficStopArea* stopArea : futureStopAreas_)
	{
		if (stopArea->Active())
		{
			inActiveFutureStopArea_ = true;
			break;
		}
	}

	Move(deltaTime);
	SetColliders();
}

void ACar::Move(const float& deltaTime)
{	
	float speedBefore_ = moveSpeed_;

	// Overtaking
	if (frameCounter_ >= 30) // Is there a better way to limit the calls to this method? It only needs to run every once in a while.
	{
		frameCounter_ = 0;

		if (!lowDetail_)
		{
			Overtake();
		}
	}

// Figuring out new speed
	// Stop at a red light
	if ((inActiveStopArea_ && inActiveFutureStopArea_)) // Should only stop completetely at red lights if both collisionRectangles are inside a stopArea  
	{																					// this prevents stopping over the line -> car will continue as futureCollisionRectangle no longer overlaps stopArea
		moveSpeed_ = FMath::Lerp(moveSpeed_, 0.0f, deltaTime * 3);					//  or too soon before the line -> car wont stop completely before collisionRectangle is also colliding
	}			
	// Slow down for entities ( & pawns ) 
	else if (blocked_ || blockedByFuturePawn_)
	{
		moveSpeed_ = FMath::Lerp(moveSpeed_, 0.0f, deltaTime * 2);
	}
	// Slow down for upcoming active stopAreas, i.e. red lights
	else if (inActiveFutureStopArea_) 
	{
		moveSpeed_ = FMath::Lerp(moveSpeed_, 150.0f, 0.66f * deltaTime);
	}
	// Normal acceleration when road is clear (or deceleration if going over target speed)
	else if (moveSpeed_ != targetSpeed_)
	{
		moveSpeed_ = FMath::Lerp(moveSpeed_, targetSpeed_, 0.46 * deltaTime);			
	}

	// If movement speed is minimal, don't move the car at all
	if (moveSpeed_ < 50)
	{
		// Used for animation control
		animAcceleration_ = FMath::Clamp(0, animAcceleration_ - deltaTime, animAcceleration_ + deltaTime); 
		return;
	}
	// Used for animation control
	animAcceleration_ = FMath::Clamp((moveSpeed_ - speedBefore_) * deltaTime * 100.0f, animAcceleration_ - deltaTime, animAcceleration_ + deltaTime);

//Applying movement
	// Figure out car new position & rotation.
	pathFollower_.Advance(moveSpeed_ * deltaTime);

	FVector tangent;
	FVector rearAxlePosition = pathFollower_.GetLocation(tangent);
	FVector middlePosition = reverseUntilPoint == -1 ?  rearAxlePosition + tangent * wheelbase_ * 0.5f : rearAxlePosition + tangent * wheelbase_ * -0.5f;
	
	if (!lowDetail_)
	{
		// Align the car with the road. Using a single line trace seems to work better than 2. Haven't tried with more.
		FHitResult hit;
		FCollisionQueryParams qp;
		qp.AddIgnoredActor(this);
		GetWorld()->LineTraceSingleByChannel(hit, middlePosition + FVector::UpVector * 75.0f, middlePosition - FVector::UpVector * 125.0f, ECollisionChannel::ECC_Pawn, qp);

		if (hit.bBlockingHit && IsValid(hit.GetActor()))
		{
			// Roads are static mesh actors so if trace hits anything else, we shouldn't do the aligning on this frame
			if (hit.GetActor()->GetClass() == AStaticMeshActor::StaticClass())
			{
				middlePosition = FVector(middlePosition.X, middlePosition.Y, hit.ImpactPoint.Z);
			}
		}
	}

	//Apply movement & rotation
	if (!tangent.ContainsNaN())
	{
		moveDirection_ = tangent;
		int currentPoint = GetPathFollower().GetCurrentLocalPoint();

		// Move forwards
		if (reverseUntilPoint == -1 && (reverseFromPoint > currentPoint || reverseFromPoint == -1)) 
		{
			SetActorLocationAndRotation(middlePosition, moveDirection_.ToOrientationQuat(), false, nullptr, ETeleportType::TeleportPhysics);
		}
		// Move in reverse (Still follow the path, the mesh just rotated 180)
		else if (reverseUntilPoint > currentPoint)
		{
			SetActorLocationAndRotation(middlePosition, (-moveDirection_).ToOrientationQuat(), false, nullptr, ETeleportType::TeleportPhysics);
		}
		// Start reversing from normal movement, until forever.
		else if (reverseFromPoint <= currentPoint && reverseFromPoint != -1)
		{
			reverseFromPoint = -1;
			reverseUntilPoint = 9999;
			SetInstantSpeed(0);
			targetSpeed_ = 150.0f;
		}
		// Stop reversing. Start moving forwards.
		else
		{
			SetInstantSpeed(0);
			reverseUntilPoint = -1;
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("WHY is this happening (in Car.cpp:Move)")); // Probably doesn't happen anymore?
		pathFollower_.NewPath(false);
	}
}

void ACar::SetColliders()
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

float ACar::GetAnimSteeringAngle()
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

void ACar::ReverseUntilPoint(int point)
{
	if (point > pathFollower_.GetCurrentLocalPoint())
	{
		reverseUntilPoint = point;
	}
}

void ACar::StartReverseFromPoint(int point)
{
	if (point > pathFollower_.GetCurrentLocalPoint() && point < pathFollower_.GetPath().keypoints.Num())
	{
		reverseFromPoint = point;
	}
}

void ACar::WaitForUnobstructedDepart(FVector collisionDimensions, FVector collisionPositionOffset)
{
	futureCollisionRectangle_.SetDimensions(collisionDimensions);
	futureCollisionRectangle_.SetPosition(GetActorLocation() - GetActorForwardVector() * collisionPositionOffset);
	futureCollisionRectangle_.SetRotation(GetActorQuat());
	waitForUnobstructedDepart = true;
}

void ACar::UpdateBlockingCollisionWith(ITrafficEntity* otherEntity)
{
	// If we're already blocked, no need to check for further blocking since it wouldn't change anything
	if (blocked_)
	{
		return;
	}

	blocked_ = BlockedBy(otherEntity);
}

void ACar::UpdateBlockingCollisionWith(FVector position)
{
	// If we're already blocked, no need to check for further blocking since it wouldn't change anything
	if (blocked_)
	{
		return;
	}

	blocked_ = BlockedBy(position);
}

void ACar::UpdatePawnCollision(CollisionRectangle pawnCollision)
{
	if (futureCollisionRectangle_.IsIntersecting(pawnCollision))
	{
		blockedByFuturePawn_ = true;
	}
}

void ACar::ClearBlockingCollisions()
{
	blocked_ = false;
	blockedByFuturePawn_ = false;
}

bool ACar::BlockedBy(ITrafficEntity* trafficEntity) const
{
	if (trafficEntity == nullptr)
	{
		return false;
	}
	
	// If other car entity is waiting to depart from a parking space, we are not blocked by that entity
	if (ACar* car = Cast<ACar>(trafficEntity))
	{
		if (car->waitForUnobstructedDepart && !waitForUnobstructedDepart)
		{
			return false;
		}
	}

	// Special case if this entity is waiting to depart from a parking space
	if (waitForUnobstructedDepart)
	{
		const CollisionRectangle& myFutureCollision = GetPredictedFutureCollisionRectangle();
		const CollisionRectangle& otherCollision = trafficEntity->GetCollisionRectangle();

		FVector dimensionSum = myFutureCollision.GetDimensions() + otherCollision.GetDimensions();

		if (FVector::DistSquared(myFutureCollision.GetPosition(), otherCollision.GetPosition()) > FVector::DotProduct(dimensionSum, dimensionSum) * 0.25f)
		{
			// If the distance between the collision rectangles is larger than half of the sum of their diagonals, they are too far away to ever intersect no matter their rotation
			return false;
		}

		// Check if future collider is intersecting with the other entity
		if (myFutureCollision.IsIntersecting(otherCollision))
		{
			// If they are intersecting, then wait, unless
			// the other entity is also waiting to depart, then
			// at least one of them should be able to depart to avoid deadlock
			if (ACar* car = Cast<ACar>(trafficEntity)) 
			{
				if (car->waitForUnobstructedDepart)
				{
					return false;
				}
			}
			return true;
		}
	}

	// Otherwise use normal collisions
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

bool ACar::BlockedBy(const FVector& point) const
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

void ACar::ApplyCustomPath(TArray<int> path, TArray<FVector> customPoints, int point, float progress)
{
	pathFollower_.SetNewCustomPath(path, customPoints); // Set the new path 
	pathFollower_.SetPointAndProgress(point, progress); // Set correct point & progress
}

void ACar::OnEnteredStopArea(ATrafficStopArea* stopArea)
{
	overlappedStopAreas_.Add(stopArea);
}

void ACar::OnExitedStopArea(ATrafficStopArea* stopArea)
{
	overlappedStopAreas_.Remove(stopArea);
}

void ACar::OnEnteredFutureStopArea(ATrafficStopArea* stopArea)
{
	futureStopAreas_.Add(stopArea);
}

void ACar::OnExitedFutureStopArea(ATrafficStopArea* stopArea)
{
	futureStopAreas_.Remove(stopArea);
}

void ACar::OnEnteredYieldArea(ATrafficYieldArea* yieldArea)
{
	bool isAlreadyInSet = false;

	overlappedYieldAreas_.Add(yieldArea, &isAlreadyInSet);

	if (!isAlreadyInSet)
	{
		shouldYield_ = true;
	}
}

void ACar::OnExitedYieldArea(ATrafficYieldArea* yieldArea)
{
	overlappedYieldAreas_.Remove(yieldArea);

	if (overlappedYieldAreas_.Num() == 0)
	{
		shouldYield_ = false;
	}
}

void ACar::OnFar()
{
	lowDetail_ = true;

	Far();
}

void ACar::OnNear()
{
	lowDetail_ = false;

	Near();
}

void ACar::Simulate(const KeypointGraph* graph)
{
	pathFollower_.Initialize(graph, this);

	// Set car speed to it's maximum, will be externally changed later
	targetSpeed_ = driverCharacteristics_.maxSpeed;

	// Allow overtaking by default
	overtakeSettings_.allow = true;
	overtakeSettings_.allowBothWays = true;

	simulate_ = true;
	useEditorTick_ = true;
}

TArray<FVector> ACar::DEBUG_GetPath()
{
	KeypointPath path = pathFollower_.GetPath();
	TArray<FVector> keypointsPos;

	for (int i = 0; i < path.keypoints.Num(); i++)
	{
		keypointsPos.Add(path.GetPointPosition(i));
	}

	return keypointsPos;
}

TArray<AActor*> ACar::GetEntitiesInArea(FVector center, FVector forward, float length)
{
	if (trafficController_ == nullptr)
	{
		return TArray<AActor*>();
	}

	return trafficController_->GetEntitiesInArea(center, forward, length, widthOfAreaCheck_);
}

TArray<float> ACar::GetEntitiesSpeedInArea(FVector center, FVector forward, float length)
{
	TArray<AActor*> entities = GetEntitiesInArea(center, forward, length);

	if (entities.Num() == 0)
	{
		return TArray<float>();
	}

	TArray<float> ret;

	for (AActor* actor : entities)
	{
		if (ACar* otherCar = Cast<ACar>(actor))
		{
			ret.Add(otherCar->GetMoveSpeed());
		}
		else if (APedestrian* otherPed = Cast<APedestrian>(actor))
		{
			ret.Add(50.0f);
		}
		else if (APawn* pawn = Cast<APawn>(actor))
		{
			/*TODO UChaosWheeledVehicleMovementComponent* chaos = pawn->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
			if (chaos)
			{
				ret.Add(chaos->GetForwardSpeed());
			}
			else
			{*/
				ret.Add(50.0f);
			//}
		}
	}

	return ret;
}

TArray<float> ACar::GetEntitiesSpeedInFront(float length)
{
	FVector center = pathFollower_.GetLocation(length * 0.5f);

	FVector temp = GetActorLocation();
	FVector forward = (center - temp);
	forward.Normalize();

	//if(IsSelected()) Debug::DrawTemporaryRect(GetWorld(), center, forward, length, widthOfAreaCheck_, 333, 25.0f, FColor::Magenta, 0.33f);

	TArray<AActor*> entities = GetEntitiesInArea(center, forward, length);
	if (entities.Num() == 0)
	{
		return TArray<float>();
	}

	TArray<float> ret;

	// If we are currently in a curve the detection may be flawed
	// => Lets only allow cars that are going to similar direction, also assume no pedestrians are on the road
	FVector myRotation = GetActorRotation().Vector();

	for (AActor* actor : entities)
	{
		if (ACar* otherCar = Cast<ACar>(actor))
		{
			FVector otherRotation = otherCar->GetActorRotation().Vector();
			float angle = FMath::Acos(FVector::DotProduct(myRotation, otherRotation)) * 90;
			if(angle <= allowedRotationDifferenceInFront)
				ret.Add(otherCar->GetMoveSpeed());
		}
		else if (APawn* pawn = Cast<APawn>(actor))
		{
			FVector otherRotation = pawn->GetActorRotation().Vector();
			float angle = FMath::Acos(FVector::DotProduct(myRotation, otherRotation)) * 90;
			if (angle <= allowedRotationDifferenceInFront)
			{
				/*TODO UChaosWheeledVehicleMovementComponent* chaos = pawn->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
				if (chaos)
				{
					ret.Add(chaos->GetForwardSpeed());
				}
				else
				{*/
					//Other PLAYER vehicles, that do not implement chaosvehicles class
				//}
			}			
		}
	}

	return ret;
}

bool ACar::Overtake()
{
	// Check if car wants to overtake (cars slow in front) & set minSpeed variable
	float currentLaneMinSpeed;

	if (!AllowedToOvertake(currentLaneMinSpeed))
	{
		return false;
	}

	// Check lanes are OK (support overtaking & long enough) & setup variables
	FVector curLaneStart, curLaneEnd, adjLaneStart, adjLaneEnd;
	float laneLength, progressToNextKeypoint;
	int adjLaneEndIndex;

	if (!GetOvertakeLanesAndParams(curLaneStart, curLaneEnd, adjLaneStart, adjLaneEnd, adjLaneEndIndex, laneLength, progressToNextKeypoint))
	{
		return false;
	}

	// Merging params, (merging as in merging to another lane)
	float progressMerge = progressToNextKeypoint + overtakeSettings_.mergeDistance / laneLength; // Progress (distance) at other lane to merge to
	float progressStayAtLane = progressToNextKeypoint + overtakeSettings_.keepLaneDistance / laneLength; // Progress (distance) to continue on current lane before starting merge
	// Point to merge to
	FVector mergeToPoint = FMath::Lerp(adjLaneStart, adjLaneEnd, progressMerge);
	
	// Check if adjacent lane is clear / it's possible to overtake without accidents
	FVector laneForwardVector = (adjLaneEnd - adjLaneStart); laneForwardVector.Normalize();

	if (!CheckAdjacentOvertakeCollisions(currentLaneMinSpeed, mergeToPoint, laneForwardVector))
	{
		return false;
	}

	// Create the new path
	FVector stayAtLanePoint = FMath::Lerp(curLaneStart, curLaneEnd, progressStayAtLane); 

	TArray<int> path; 
	TArray<FVector> customPoints;
	CreateOvertakePath(path, customPoints, adjLaneEndIndex, stayAtLanePoint, mergeToPoint);

	// And apply it
	ApplyCustomPath(path, customPoints, pathFollower_.GetCurrentLocalPoint(), 1.0f);

	return true;
}

bool ACar::AllowedToOvertake(float& laneMinSpeed)
{
	TArray<float> speedsInFront = GetEntitiesSpeedInFront(frontalSightLength_); // Get any entities speed that are in front of the car

	if (speedsInFront.Num() > 0)
	{
		laneMinSpeed = FMath::Min(speedsInFront);

		if (overtakeSettings_.allow && laneMinSpeed < targetSpeed_ * overtakeSettings_.maxSpeedDiffMultiplier)
		{
			return true;
		}
	}

	return false;
}

bool ACar::GetOvertakeLanesAndParams(FVector& laneStart, FVector& laneEnd, FVector& secondLaneStart, FVector& secondLaneEnd, int& secondLaneEndIndex, float& laneLength, float& progressOnLane)
{
	KeypointPath path = pathFollower_.GetPath();
	int currentKeypoint = pathFollower_.GetCurrentLocalPoint();

	// Not enough route left
	if (path.keypoints.Num() <= currentKeypoint + 1)
	{
		return false;
	}

	// Does the current lane support overtaking?
	int laneStartIndex = path.keypoints[currentKeypoint];
	int laneEndIndex = path.keypoints[currentKeypoint + 1];
	std::pair overtakeLane = path.graph->GetOvertakePair(std::make_pair(laneStartIndex, laneEndIndex), overtakeSettings_.allowBothWays);
	int secondLaneStartIndex = overtakeLane.first;
	secondLaneEndIndex = overtakeLane.second; // Needed when creating path for overtaking, and thus passed as a reference

	if (secondLaneStartIndex == -1 || secondLaneEndIndex == -1)
	{
		return false;
	}

	// Keypoint locations for the lanes, we can use KeypointGraph::GetKeypointPosition, since the indexes get verified in GetOvertakePair
	laneStart = path.graph->GetKeypointPosition(laneStartIndex);
	laneEnd = path.graph->GetKeypointPosition(laneEndIndex);
	secondLaneStart = path.graph->GetKeypointPosition(secondLaneStartIndex);
	secondLaneEnd = path.graph->GetKeypointPosition(secondLaneEndIndex);

	laneLength = FVector::Dist2D(laneStart, laneEnd);
	progressOnLane = FVector::Dist2D(laneStart, GetActorLocation()) / laneLength;

	// Check that theres enough space left to start overtaking / merge
	return laneLength * (1 - progressOnLane) >= overtakeSettings_.mergeDistance;
}

bool ACar::CheckAdjacentOvertakeCollisions(float minSpeed, FVector laneCenter, FVector laneForward)
{
	// Compare adjacent lane cars that are behind to current speed, because
	// car might have to accelerate too fast if compared with target speed
	FVector checkCenter = laneCenter + overtakeSettings_.collisionCheckOffsetBehind * laneForward;
	float maxIncomingSpeed = GetEntitiesSpeedInArea(checkCenter, laneForward, overtakeSettings_.collisionCheckLengthBehind).Max();

	if (maxIncomingSpeed * overtakeSettings_.maxSpeedDiffMultiplierBehind > moveSpeed_)
	{
		if (IsSelected())
		{
			Debug::Log("Something coming too fast behind");
			//Debug::DrawTemporaryRect(GetWorld(), checkCenter, laneForward, overtakeSettings_.collisionCheckLengthBehind, widthOfAreaCheck_, 250, 25.0f, FColor::Red, 1.0f);
		}

		return false;
	}

	// Abort overtake if any traffic on the side of this car
	checkCenter = laneCenter + overtakeSettings_.collisionCheckOffsetSide * laneForward;	

	if (GetEntitiesInArea(checkCenter, laneForward, overtakeSettings_.collisionCheckLengthSide).Num() != 0)
	{
		if (IsSelected())
		{
			Debug::Log("Something on the side");
			//Debug::DrawTemporaryRect(GetWorld(), checkCenter, laneForward, overtakeSettings_.collisionCheckLengthSide, widthOfAreaCheck_, 250, 25.0f, FColor::Blue, 1.0f);
		}

		return false;
	}

	// Abort overtaking if adjacent lane is going slower (or just slightly faster) than current lane 
	checkCenter = laneCenter + overtakeSettings_.collisionCheckOffsetFront * laneForward;
	int minIndex;
	float adjLaneMinSpeed = FMath::Min(GetEntitiesSpeedInArea(checkCenter, laneForward, overtakeSettings_.collisionCheckLengthFront), &minIndex);

	if (adjLaneMinSpeed > minSpeed * overtakeSettings_.minSpeedDiffMultiplierFront && minIndex != INDEX_NONE)
	{
		if (IsSelected())
		{
			UE_LOG(LogTemp, Warning, TEXT("Other lane going slower"));
			//Debug::DrawTemporaryRect(GetWorld(), checkCenter, laneForward, overtakeSettings_.collisionCheckLengthFront, widthOfAreaCheck_, 250, 25.0f, FColor::Orange, 1.0f);
		}

		return false;
	}

	return true;
}

void ACar::CreateOvertakePath(TArray<int>& path, TArray<FVector>& customPoints, int adjLaneEndIndex, FVector stayAtLanePoint, FVector mergeToPoint)
{
	KeypointPath oldPath = pathFollower_.GetPath();
	int currentKeypoint = pathFollower_.GetCurrentLocalPoint();

	// Get already traversed path, new path will be added on top of that, so we can still see where the car came from
	int customIndex = -8; // start indexing custom points from -8

	for (int i = 0; i < currentKeypoint; i++)
	{
		if (oldPath.keypoints[i] <= -8)
		{
			customPoints.Add(oldPath.GetPointPosition(i));
			path.Add(customIndex--);
		}
		else
		{
			path.Add(oldPath.keypoints[i]);
		}
	}

	// Create the new path
	TArray<int> newPath = oldPath.graph->GetRandomPathFrom(adjLaneEndIndex).keypoints;
	FVector currentLocation = GetActorLocation();

	customPoints.Add(currentLocation - GetActorForwardVector() * wheelbase_); // A location bit before current location
	newPath.Insert(customIndex--, 0);

	customPoints.Add(currentLocation); // my current location
	newPath.Insert(customIndex--, 1);

	customPoints.Add(stayAtLanePoint); // Car will keep on current lane for a while before starting turning (allows to set blinkers etc. bit before turning happens)
	newPath.Insert(customIndex--, 2);

	customPoints.Add(mergeToPoint);	// Location car will merge to on adjacent lane
	newPath.Insert(customIndex--, 3);

	//Append new path to already traversed part of the old path
	path.Append(newPath);
}
