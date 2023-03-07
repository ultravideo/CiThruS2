#include "Pedestrian.h"
#include "TrafficController.h"
#include "TrafficStopArea.h"
#include "../Debug.h"

#include "Math/UnrealMathUtility.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
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

FVector APedestrian::PreferredSpawnPositionOffset()
{
	// This is needed because Unreal Engine characters have their origin at their center instead
	// of at their feet so they're spawned inside the ground if they're spawned at the keypoints
	return FVector::UpVector * HEIGHT_CM * 0.5f;
}

void APedestrian::Simulate()
{
	// For some reason SpawnDefaultActor() does nothing half the time so we have to spawn
	// a controller manually
	Controller = GetWorld()->SpawnActor<AAIController>();
	Controller->Possess(this);

	GetCharacterMovement()->MaxWalkSpeed = moveSpeed_;
	GetCharacterMovement()->bUseRVOAvoidance = true;

	NewPath(true);

	useEditorTick_ = true;
}

void APedestrian::NewPath(const bool& fromNearestKeypoint)
{
	if (trafficController_ == nullptr)
	{
		return;
	}

	const KeypointGraph& graph = trafficController_->GetSharedUseGraph();

	if (fromNearestKeypoint)
	{
		pathFollower_.SetPath(graph.GetRandomPathFrom(graph.GetClosestKeypoint(GetActorLocation())));
	}
	else
	{
		// Spawn only at the allocated spawn points
		int spawnPoint = graph.GetRandomSpawnPoint();

		pathFollower_.SetPath(graph.GetRandomPathFrom(spawnPoint));
		SetActorLocation(graph.GetKeypointPosition(spawnPoint) + PreferredSpawnPositionOffset());
	}
}

void APedestrian::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

	// Check if any overlapped stop areas are active
	inActiveStopArea_ = false;

	for (ATrafficStopArea* stopArea : overlappedStopAreas_)
	{
		// Also check that the stop area is pointing in the direction we're trying to go in
		if (stopArea->Active() && FVector2D::DotProduct(FVector2D(stopArea->GetActorForwardVector()), FVector2D(moveDirection_)) > 0.0f)
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
		// No need to move
		aiController->StopMovement();

		return;
	}

	if (pathFollower_.FullyTraversed())
	{
		NewPath(false);
	}

	EPathFollowingRequestResult::Type pathFollowResult = aiController->MoveToLocation(pathFollower_.GetCurrentTarget(), 50.0f, true, true, true);

	if (pathFollowResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		pathFollower_.AdvanceTarget();
	}

	moveDirection_ = GetActorForwardVector();

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

CollisionRectangle APedestrian::GetPredictedFutureCollisionRectangle() const
{
	// Predict where this pedestrian will be in the future
	return CollisionRectangle(collisionRectangle_.GetDimensions(),
		GetActorLocation() + moveDirection_ * (collisionDimensions_.X + collisionDimensions_.Y) * 0.5f, GetActorRotation().Quaternion());
}
