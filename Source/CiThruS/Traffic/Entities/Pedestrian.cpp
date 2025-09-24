#include "Pedestrian.h"
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

APedestrian::APedestrian()
{
	GetCapsuleComponent()->AddLocalOffset(FVector::UpVector * HEIGHT_CM * 0.5f);

	AutoPossessAI = EAutoPossessAI::Disabled;

	// Set this actor to call Tick() every frame
	PrimaryActorTick.bCanEverTick = true;
}

void APedestrian::BeginPlay()
{
	Super::BeginPlay();

	collisionRectangle_.SetDimensions(collisionDimensions_);
	collisionRectangle_.SetPosition(GetActorLocation());
	collisionRectangle_.SetRotation(GetActorRotation().Quaternion());
}

void APedestrian::Destroyed()
{
	if (trafficController_ != nullptr)
	{
		trafficController_->InvalidateTrafficEntity(this);
	}

	Super::Destroyed();
}

FVector APedestrian::PreferredSpawnPositionOffset()
{
	// This is needed because Unreal Engine characters have their origin at their center instead
	// of at their feet so they're spawned inside the ground if they're spawned exactly at the keypoints
	return FVector::UpVector * HEIGHT_CM * 0.5f;
}

void APedestrian::Simulate(const KeypointGraph* graph)
{
	// For some reason SpawnDefaultActor() does nothing half the time so we have to spawn a controller manually
	Controller = GetWorld()->SpawnActor<AAIController>();
	Controller->Possess(this);

	GetCharacterMovement()->MaxWalkSpeed = moveSpeed_;
	GetCharacterMovement()->bUseRVOAvoidance = true;

	pathFollower_.Initialize(graph, this, FVector::UpVector * HEIGHT_CM * 0.5f);

	useEditorTick_ = true;
	simulate_ = true;
}

void APedestrian::Tick(float deltaTime)
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
			break;
		}
	}

	AAIController* aiController = Cast<AAIController>(Controller);

	if (aiController == nullptr)
	{
		return;
	}

	if (Stopped())
	{
		// inActiveStopArea_ = true;
		// No need to move
		aiController->StopMovement();

		return;
	}

	EPathFollowingRequestResult::Type pathFollowResult = aiController->MoveToLocation(pathFollower_.GetLocation(), 50.0f, true, true, true, true, nullptr, false);

	if (pathFollowResult == EPathFollowingRequestResult::AlreadyAtGoal || pathFollowResult == EPathFollowingRequestResult::Failed)
	{
		// Reached current target point: get next target point or new path if end reached.
		pathFollower_.AdvanceTarget();
	}
	else if (aiController->GetPathFollowingComponent()->GetStatus() == EPathFollowingStatus::Moving && !pathFollower_.IsLastTarget())
	{
/* 		boing_ = true;
		// Calculate a new target location slightly before reaching the current goal. Reduces weird movement.
		// Doesn't get rid of the brief stop when calculating a new target though.
		FVector goalLocation = aiController->GetTargetLocation();
		UE_LOG(LogTemp, Warning, TEXT("HEP! aiController->GetTargetLocation is %s"), *goalLocation.ToString());
		float distToGoal = FVector::Dist2D(goalLocation, GetActorLocation());
		if (distToGoal <= 100.0f)
		{
			pathFollower_.AdvanceTarget();
		} */
	}
	
	// Update pathfinding status
	pathFollowerGoal_ = pathFollower_.GetLocation();
	distanceToFollowerGoal_ = FVector::Dist2D(pathFollowerGoal_, GetActorLocation());
	controllerGoal_ = aiController->GetTargetLocation();
	distanceToControllerGoal_ = FVector::Dist2D(controllerGoal_, GetActorLocation());

	if (pathFollowResult == EPathFollowingRequestResult::AlreadyAtGoal)
		atGoal_ = true; else atGoal_ = false;

	if (aiController->GetPathFollowingComponent()->GetStatus() == EPathFollowingStatus::Moving)
		moving_ = true; else moving_ = false;
	
	lastTarget_ = pathFollower_.IsLastTarget();

	pathFollowingRqResult_ = UEnum::GetValueAsString(pathFollowResult);
	pathFollowingStatus_ = UEnum::GetValueAsString(aiController->GetPathFollowingComponent()->GetStatus());

	moveDirection_ = GetActorForwardVector();

	// Update collision
	collisionRectangle_.SetPosition(GetActorLocation());
	collisionRectangle_.SetRotation(GetActorRotation().Quaternion());
}

void APedestrian::OnEnteredStopArea(ATrafficStopArea* stopArea)
{
	overlappedStopAreas_.insert(stopArea);
}

void APedestrian::OnExitedStopArea(ATrafficStopArea* stopArea)
{
	overlappedStopAreas_.erase(stopArea);
}

void APedestrian::OnEnteredYieldArea(ATrafficYieldArea* yieldArea)
{
	bool isAlreadyInSet = false;

	overlappedYieldAreas_.Add(yieldArea, &isAlreadyInSet);

	if (!isAlreadyInSet)
	{
		shouldYield_ = true;
	}
}

void APedestrian::OnExitedYieldArea(ATrafficYieldArea* yieldArea)
{
	overlappedYieldAreas_.Remove(yieldArea);

	if (overlappedYieldAreas_.Num() == 0)
	{
		shouldYield_ = false;
	}
}

void APedestrian::OnFar()
{
	Far();
}

void APedestrian::OnNear()
{
	Near();
}

CollisionRectangle APedestrian::GetPredictedFutureCollisionRectangle() const
{
	// Predict where this pedestrian will be in the future
	return CollisionRectangle(collisionRectangle_.GetDimensions(),
		GetActorLocation() + moveDirection_ * (collisionDimensions_.X + collisionDimensions_.Y) * 0.5f, GetActorRotation().Quaternion());
}
