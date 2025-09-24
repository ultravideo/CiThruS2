#pragma once

#include "Traffic/Paths/FreePathFollower.h"
#include "Traffic/Entities/ITrafficEntity.h"

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include <set>

#include "Pedestrian.generated.h"

class ATrafficController;
class ATrafficStopArea;
class ATrafficYieldArea;

// A simulated pedestrian that follows shared-use paths and obeys traffic lights
UCLASS(Abstract)
class CITHRUS_API APedestrian : public ACharacter, public ITrafficEntity
{
	GENERATED_BODY()

public:
	virtual FString GetName() const override { return ACharacter::GetName(); }
	
	static FVector PreferredSpawnPositionOffset();

	void Simulate(const KeypointGraph* graph);

	virtual void Tick(float deltaTime) override;
	inline virtual bool ShouldTickIfViewportsOnly() const override { return useEditorTick_; }

	inline virtual void SetController(ATrafficController* controller) override { trafficController_ = controller; }

	virtual void OnEnteredStopArea(ATrafficStopArea* stopArea) override;
	virtual void OnExitedStopArea(ATrafficStopArea* stopArea) override;

	virtual void OnEnteredYieldArea(ATrafficYieldArea* stopArea) override;
	virtual void OnExitedYieldArea(ATrafficYieldArea* stopArea) override;

	// These functions are empty because currently pedestrians don't care about collisions
	virtual void UpdateBlockingCollisionWith(ITrafficEntity* otherEntity) override { }
	virtual void UpdateBlockingCollisionWith(FVector position) override { }

	virtual void ClearBlockingCollisions() { }

	inline virtual bool Yielding() const override { return shouldYield_; }
	inline virtual bool Stopped() const override { return inActiveStopArea_; }
	inline virtual bool Blocked() const { return true; } // TODO: Hack to prevent pedestrians from dropping bicycle speeds to min, actually need to implement proper blocking

	virtual void OnFar() override;
	virtual void OnNear() override;

	inline virtual FVector GetMoveDirection() const override { return moveDirection_; }
	inline virtual CollisionRectangle GetCollisionRectangle() const override { return collisionRectangle_; }
	virtual CollisionRectangle GetPredictedFutureCollisionRectangle() const override;

	ATrafficController* GetController() const { return trafficController_; }

protected:
	APedestrian();

	ATrafficController* trafficController_;
	FreePathFollower pathFollower_;
	CollisionRectangle collisionRectangle_;

	bool simulate_ = false;
	bool useEditorTick_ = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Pedestrian")
	float moveSpeed_ = 200.0f;


	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Traffic System|Pathfinding Status")
	FVector pathFollowerGoal_ = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Traffic System|Pathfinding Status")
	float distanceToFollowerGoal_ = 0.0f;
 
//	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Traffic System|Pathfinding Status")
	FVector controllerGoal_ = FVector::ZeroVector;
//	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Traffic System|Pathfinding Status")
	float distanceToControllerGoal_ = 0.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Traffic System|Pathfinding Status")
	bool atGoal_ = false;
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Traffic System|Pathfinding Status")
	bool moving_ = false;
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Traffic System|Pathfinding Status")
	bool lastTarget_ = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Traffic System|Pathfinding Status")
	FString pathFollowingRqResult_ = "";

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Traffic System|Pathfinding Status")
	FString pathFollowingStatus_ = "";

	//UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Traffic System|Pathfinding Status")
	bool boing_ = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Traffic System|Collision Status")
	bool shouldYield_;
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Traffic System|Collision Status")
	bool inActiveStopArea_;

	std::set<ATrafficStopArea*> overlappedStopAreas_;
	TSet<ATrafficYieldArea*> overlappedYieldAreas_;

	FVector moveDirection_;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Pedestrian")
	FVector collisionDimensions_ = FVector(100.0f, 100.0f, 180.0f);

	UFUNCTION(BlueprintNativeEvent)
	void Far();
	void Far_Implementation() { }

	UFUNCTION(BlueprintNativeEvent)
	void Near();
	void Near_Implementation() { }

	virtual void BeginPlay() override;
	virtual void Destroyed() override;
};
