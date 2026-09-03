#include "Pedestrian.h"
#include "Traffic/TrafficController.h"
#include "Traffic/Areas/TrafficStopArea.h"
#include "Misc/Debug.h"

#include "Math/UnrealMathUtility.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

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
		// Important so that the simulation doesn't crash if traffic entities are randomly deleted e.g. by the user
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
	AAIController* controller = GetWorld()->SpawnActor<AAIController>();
	controller->Possess(this);

	GetCharacterMovement()->MaxWalkSpeed = moveSpeed_;

	// Makes pedestrians avoid walking into each other
	GetCharacterMovement()->AvoidanceConsiderationRadius = 300.0f;
	GetCharacterMovement()->AvoidanceWeight = 0.5f;
	GetCharacterMovement()->SetAvoidanceEnabled(true);

	pathFollower_.Initialize(graph, this, FVector::UpVector * HEIGHT_CM * 0.5f);

	useEditorTick_ = true;
	simulate_ = true;

	GoToNextTarget();
}

void APedestrian::Tick(float deltaTime)
{
	Super::Tick(deltaTime);
	UpdateZone(GetController());

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

	bool isCurrentlyStopped = Stopped();

	if (isCurrentlyStopped && !wasPreviouslyStopped_)
		aiController->StopMovement();
	else if (!isCurrentlyStopped && wasPreviouslyStopped_)
		GoToNextTarget();
	
	wasPreviouslyStopped_ = isCurrentlyStopped;


	// Update editor pathfinding status display
	pathFollowerGoal_ = pathFollower_.GetLocation();
	distanceToFollowerGoal_ = FVector::Dist2D(pathFollowerGoal_, GetActorLocation());

	atGoal_ = pathFollowResult_ == EPathFollowingRequestResult::AlreadyAtGoal;
	moving_ = aiController->GetPathFollowingComponent()->GetStatus() == EPathFollowingStatus::Moving;
	
	lastTarget_ = pathFollower_.IsLastTarget();

	pathFollowingRqResult_ = UEnum::GetValueAsString(pathFollowResult_);
	pathFollowingStatus_ = UEnum::GetValueAsString(aiController->GetPathFollowingComponent()->GetStatus());

	moveDirection_ = GetActorForwardVector();

	// Update collision
	collisionRectangle_.SetPosition(GetActorLocation());
	collisionRectangle_.SetRotation(GetActorRotation().Quaternion());
}

void APedestrian::GoToNextTarget()
{
    AAIController* aiController = Cast<AAIController>(Controller);
    if (aiController)
    {
        // Unbind previous delegates to be safe
        aiController->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
        
        // Bind to the finished event
        aiController->GetPathFollowingComponent()->OnRequestFinished.AddUObject(this, &APedestrian::OnMoveCompleted);
        
        pathFollowResult_ = aiController->MoveToLocation(pathFollower_.GetLocation() + FVector::UpVector * 50.0f, 50.0f, true, true, true, false, nullptr, false);
    }
}

// This fires automatically when they reach the goal, fail, or are interrupted
void APedestrian::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{

	// If pathfinding aborted because of stopping, don't do anything. 
	// Tick function will handle restarting once the block clears. 
	if (Stopped()) return;

    if (Result.IsSuccess())
    {
        pathFollower_.AdvanceTarget();
        GoToNextTarget(); // Start moving to the next one
    }
    else if (Result.HasFlag(FPathFollowingResultFlags::Blocked))
	{
		// Blocked means there's something in the way temporarily
		GoToNextTarget(); // Try again, otherwise the pedestrian will never move again even if the path is cleared
	}
	else
    {
        // Handle failure (e.g., target position is not on the nav mesh)
        UE_LOG(LogTemp, Warning, TEXT("Pedestrian path failed"));
    }
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

void APedestrian::Visualize(float duration) const
{
	collisionRectangle_.Visualize(GetWorld(), duration);

	const FVector rayBegin = collisionRectangle_.GetPosition();
	const FVector rayEnd = rayBegin + moveDirection_ * 250.0f;

	Debug::DrawTemporaryLine(GetWorld(), rayBegin, rayEnd, FColor::Blue, duration * 1.1f, 5.0f);
}
