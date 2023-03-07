#pragma once

#include "TrafficController.h"
#include "PathFollower.h"
#include "CollisionRectangle.h"
#include "ITrafficEntity.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPCar.generated.h"

class ATrafficStopArea;
class APedestrian;

UCLASS()
class HERVANTAUE4_API ANPCar : public AActor, public ITrafficEntity
{
	GENERATED_BODY()
	
public:	
	void Simulate();

	bool CollidingWith(ITrafficEntity* trafficEntity) const;
	bool CollidingWith(const FVector& point) const;

	inline virtual void SetController(ATrafficController* controller) override { trafficController_ = controller; };

	void NewPath(const bool& fromNearestKeypoint);

	void AddBlockingActor(AActor* actor);
	void RemoveBlockingActor(AActor* actor);

	virtual void OnEnteredStopArea(ATrafficStopArea* stopArea) override;
	virtual void OnExitedStopArea(ATrafficStopArea* stopArea) override;

	virtual void OnEnteredYieldArea(ATrafficYieldArea* yieldArea) override;
	virtual void OnExitedYieldArea(ATrafficYieldArea* yieldArea) override;

	virtual bool Yielding() const override { return shouldYield_; };
	virtual inline bool Stopped() const override { return blockingActors_.Num() > 0 || inActiveStopArea_; };

	virtual FVector GetMoveDirection() const override { return moveDirection_; };
	virtual CollisionRectangle GetCollisionRectangle() const override { return collisionRectangle_; };
	virtual CollisionRectangle GetPredictedFutureCollisionRectangle() const override;

	virtual void Tick(float deltaTime) override;

	inline virtual bool ShouldTickIfViewportsOnly() const override { return useEditorTick_; };

protected:
	ANPCar();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle collision vision")
	float narrowFrontConeAngle_ = 12.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle collision vision")
	float narrowFrontConeLength_ = 2000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle collision vision")
	float wideFrontConeAngle_ = 30.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle collision vision")
	float wideFrontConeLength_ = 750.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle collision vision")
	float wideLeftRightConeAngle_ = 60.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle collision vision")
	float wideLeftRightConeLength_ = 500.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle properties")
	float moveSpeed_ = 500.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle properties")
	float wheelbase_ = 300.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle properties")
	FVector collisionDimensions_ = FVector(500.0f, 200.0f, 150.0f);

	bool useEditorTick_ = false;

	PathFollower pathFollower_;
	CollisionRectangle collisionRectangle_;

	bool shouldYield_;
	bool inActiveStopArea_;

	TArray<AActor*> blockingActors_;
	FCriticalSection blockListMutex_;

	TSet<ATrafficStopArea*> overlappedStopAreas_;
	TSet<ATrafficYieldArea*> overlappedYieldAreas_;

	ATrafficController* trafficController_;

	FVector moveDirection_;

	virtual void BeginPlay() override;

	void Move(const float& deltaTime);
};
