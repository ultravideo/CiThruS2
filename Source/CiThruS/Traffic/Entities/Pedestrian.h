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

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float moveSpeed_ = 200.0f;

	bool shouldYield_;
	bool inActiveStopArea_;

	std::set<ATrafficStopArea*> overlappedStopAreas_;
	TSet<ATrafficYieldArea*> overlappedYieldAreas_;

	FVector moveDirection_;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle properties")
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
