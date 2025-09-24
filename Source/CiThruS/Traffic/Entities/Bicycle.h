#pragma once

#include "Traffic/Paths/CurvePathFollower.h"
#include "Traffic/Entities/ITrafficEntity.h"

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include <set>

#include "Bicycle.generated.h"

class ATrafficController;
class ATrafficStopArea;
class ATrafficYieldArea;

// A simulated pedestrian that follows shared-use paths and obeys traffic lights
UCLASS(Abstract)
class CITHRUS_API ABicycle : public AActor, public ITrafficEntity
{
	GENERATED_BODY()

public:
	virtual FString GetName() const override { return AActor::GetName(); }
	
	static FVector PreferredSpawnPositionOffset();

	void Simulate(const KeypointGraph* graph);

	virtual void Tick(float deltaTime) override;
	inline virtual bool ShouldTickIfViewportsOnly() const override { return useEditorTick_; }

	inline virtual void SetController(ATrafficController* controller) override { trafficController_ = controller; }

	inline CurvePathFollower GetPathFollower() const { return pathFollower_; }

	virtual void OnEnteredStopArea(ATrafficStopArea* stopArea) override;
	virtual void OnExitedStopArea(ATrafficStopArea* stopArea) override;

	virtual void OnEnteredYieldArea(ATrafficYieldArea* stopArea) override;
	virtual void OnExitedYieldArea(ATrafficYieldArea* stopArea) override;

	virtual void UpdateBlockingCollisionWith(ITrafficEntity* otherEntity) override;
	virtual void UpdateBlockingCollisionWith(FVector position) override;
	virtual void UpdatePawnCollision(CollisionRectangle pawnCollision) override;

	virtual void ClearBlockingCollisions() override;

	inline virtual bool Yielding() const override { return shouldYield_; }
	inline virtual bool Stopped() const override { return inActiveStopArea_; }
	inline virtual bool Blocked() const { return blocked_ || blockedByFuturePawn_; }

	virtual void OnFar() override;
	virtual void OnNear() override;

	inline virtual FVector GetMoveDirection() const override { return moveDirection_; }
	inline virtual CollisionRectangle GetCollisionRectangle() const override { return collisionRectangle_; }
	inline virtual CollisionRectangle GetPredictedFutureCollisionRectangle() const override { return futureCollisionRectangle_; }

	ATrafficController* GetController() const { return trafficController_; }

protected:
	ABicycle();

	void SetColliders();

	ATrafficController* trafficController_;
	CurvePathFollower pathFollower_;
	CollisionRectangle collisionRectangle_;
	CollisionRectangle futureCollisionRectangle_;

	bool simulate_ = false;
	bool useEditorTick_ = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Bicycle")
	float baseSpeed_ = 200.0f;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Transient, Category = "Traffic System|Bicycle")
	float randomSpeed_ = 20.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Traffic System|Bicycle")
	float moveSpeed_ = 200.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Transient, Category = "Traffic System|Bicycle")
	float currentTurnAmount_ = 0.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Transient, Category = "Traffic System|Collision Status")
	bool blocked_;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Transient, Category = "Traffic System|Collision Status")
	bool blockedByFuturePawn_;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Transient, Category = "Traffic System|Collision Status")
	bool shouldYield_;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Transient, Category = "Traffic System|Collision Status")
	bool inActiveStopArea_;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Transient, Category = "Traffic System|Collision Status")
	FString blockingEntity_ = "";

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Transient, Category = "Traffic System|Collision Status")
	FVector blockingPosition_ = FVector::ZeroVector;

	std::set<ATrafficStopArea*> overlappedStopAreas_;
	TSet<ATrafficYieldArea*> overlappedYieldAreas_;

	FVector moveDirection_;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Bicycle")
	FVector collisionDimensions_ = FVector(120.0f, 50.0f, 200.0f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Traffic System|Bicycle")
	float wheelbase_ = 100.0f;

	UFUNCTION(BlueprintNativeEvent)
	void Far();
	void Far_Implementation() {}

	UFUNCTION(BlueprintNativeEvent)
	void Near();
	void Near_Implementation() {}

	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	bool BlockedBy(ITrafficEntity* trafficEntity) const;
	bool BlockedBy(const FVector& point) const;
};
