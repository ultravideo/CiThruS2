#pragma once

#include "PathFollower.h"
#include "ITrafficEntity.h"

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include <set>

#include "Pedestrian.generated.h"

class ATrafficController;
class ATrafficStopArea;
class ATrafficYieldArea;

UCLASS()
class HERVANTAUE4_API APedestrian : public ACharacter, public ITrafficEntity
{
	GENERATED_BODY()

public:
	static FVector PreferredSpawnPositionOffset();

	void Simulate();
	void NewPath(const bool& fromNearestKeypoint);

	virtual void Tick(float deltaTime) override;
	inline virtual bool ShouldTickIfViewportsOnly() const override { return useEditorTick_; };

	inline virtual void SetController(ATrafficController* controller) override { trafficController_ = controller; };

	virtual void OnEnteredStopArea(ATrafficStopArea* stopArea) override;
	virtual void OnExitedStopArea(ATrafficStopArea* stopArea) override;

	virtual void OnEnteredYieldArea(ATrafficYieldArea* stopArea) override;
	virtual void OnExitedYieldArea(ATrafficYieldArea* stopArea) override;

	inline virtual bool Yielding() const override { return shouldYield_; };
	inline virtual bool Stopped() const override { return inActiveStopArea_; };

	inline virtual FVector GetMoveDirection() const override { return moveDirection_; };
	inline virtual CollisionRectangle GetCollisionRectangle() const override { return collisionRectangle_; };
	virtual CollisionRectangle GetPredictedFutureCollisionRectangle() const override;

protected:
	APedestrian();

	float cautionDistance_ = 700.0f;

	ATrafficController* trafficController_;
	PathFollower pathFollower_;
	CollisionRectangle collisionRectangle_;

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

	virtual void BeginPlay() override;
};
