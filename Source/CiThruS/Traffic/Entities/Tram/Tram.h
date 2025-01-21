#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Traffic/Entities/ITrafficEntity.h"
#include "Traffic/Entities/Car.h"
#include "Tram.generated.h"

class ATrafficStopArea;

// A simulated tram that follows tram tracks
UCLASS(Abstract)
class CITHRUS_API ATram : public AActor, public ITrafficEntity
{
	GENERATED_BODY()

public:	
	inline void SetMoveSpeed(float speed) { targetSpeed_ = speed; }

	// Called every frame
	virtual void Tick(float deltaTime) override;

	virtual void Simulate(const KeypointGraph* graph);

	inline virtual void SetController(ATrafficController* controller) override { trafficController_ = controller; }

	virtual void OnEnteredStopArea(ATrafficStopArea* stopArea) override;
	virtual void OnExitedStopArea(ATrafficStopArea* stopArea) override;

	virtual void OnEnteredYieldArea(ATrafficYieldArea* yieldArea) override { /* Do nothing because the tram doesn't yield */ }
	virtual void OnExitedYieldArea(ATrafficYieldArea* yieldArea) override { /* Do nothing because the tram doesn't yield */ }

	virtual void OnFar() override;
	virtual void OnNear() override;

	virtual void UpdateBlockingCollisionWith(ITrafficEntity* otherEntity) override;
	virtual void UpdateBlockingCollisionWith(FVector position) override;

	virtual void ClearBlockingCollisions() override;

	// Tram should always have priority over cars
	inline virtual bool Yielding() const override { return false; }
	inline virtual bool Stopped() const override { return blocked_ || inActiveStopArea_; }

	inline virtual FVector GetMoveDirection() const override { return moveDirection_; }

	inline virtual CollisionRectangle GetCollisionRectangle() const override { return collisionRectangle_; }
	inline virtual CollisionRectangle GetPredictedFutureCollisionRectangle() const override { return futureCollisionRectangle_; }

	// Animation stuff - not in use currently? Probably not needed either?
	UFUNCTION(BlueprintCallable)
	float GetAnimWheelRotationSpeed() { return moveSpeed_ >= 50 ? moveSpeed_ / 100 : 0.0f; }
	UFUNCTION(BlueprintCallable)
	float GetAnimSteeringAngle();
	UFUNCTION(BlueprintCallable)
	float GetAccelerationSpeed() { return animAcceleration_; }
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vehicle Animations")
	bool enableAnimation = true;

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle properties")
	float targetSpeed_ = 500.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle properties")
	float moveSpeed_ = 500.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle properties")
	float wheelbase_ = 250.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle properties")
	FVector collisionDimensions_ = FVector(500.0f, 200.0f, 150.0f);

	// 'Physical' cart components in BP - has to be set in BP constructor
	UPROPERTY(BluePrintReadWrite)
	TArray<USceneComponent*> carts_;

	// Distance between neighbouring carts
	UPROPERTY(BluePrintReadWrite, EditAnywhere, Category = "Tram specific properties")
	float cartGap_ = 100;

	UFUNCTION(BlueprintNativeEvent)
	void Far();
	void Far_Implementation() { };

	UFUNCTION(BlueprintNativeEvent)
	void Near();
	void Near_Implementation() { };

	bool simulate_ = false;
	bool useEditorTick_ = false;

	CurvePathFollower pathFollower_;
	CollisionRectangle collisionRectangle_;
	CollisionRectangle futureCollisionRectangle_;

	float collisionBoundingSphereRadius_;

	bool inActiveStopArea_;
	bool blocked_;

	TSet<ATrafficStopArea*> overlappedStopAreas_;

	ATrafficController* trafficController_;

	FVector moveDirection_;
	float animAcceleration_ = 0.0f;

	ATram();

	virtual void Move(const float& deltaTime);
	virtual void SetColliders();

	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	bool BlockedBy(ITrafficEntity* trafficEntity) const;
	bool BlockedBy(const FVector& point) const;
};
