#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Components/SplineMeshComponent.h"
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
	virtual FString GetName() const override { return AActor::GetName(); }

	inline void SetMoveSpeed(float speed) { targetSpeed_ = speed; }

	inline ATrafficController* GetController() const { return trafficController_; }

	// Called every frame
	virtual void Tick(float deltaTime) override;

	virtual void Simulate(const KeypointGraph* graph, int seed);

	inline virtual void SetController(ATrafficController* controller) override { trafficController_ = controller; }

	UFUNCTION(BlueprintCallable)
	virtual int GetVariantId() const override { return BpGetVariantId(); }
	UFUNCTION(BlueprintCallable)
	virtual void SetVariantId(int variantId) override { BpSetVariantId(variantId); }
	UFUNCTION(BlueprintCallable)
	virtual int NumVariants() const override { return BpNumVariants(); }

	virtual void OnEnteredStopArea(ATrafficStopArea* stopArea) override;
	virtual void OnExitedStopArea(ATrafficStopArea* stopArea) override;

	virtual void OnEnteredYieldArea(ATrafficYieldArea* yieldArea) override { /* Do nothing because the tram doesn't yield */ }
	virtual void OnExitedYieldArea(ATrafficYieldArea* yieldArea) override { /* Do nothing because the tram doesn't yield */ }

	virtual void OnFar() override { BpOnFar(); }
	virtual void OnNear() override { BpOnNear(); }

	virtual void UpdateBlockingCollisionWith(ITrafficEntity* otherEntity) override;
	virtual void UpdateBlockingCollisionWith(FVector position) override;

	virtual void ClearBlockingCollisions() override;

	// Tram should always have priority over cars
	inline virtual bool Yielding() const override { return false; }
	inline virtual bool Stopped() const override { return blocked_ || inActiveStopArea_; }
	inline virtual bool Blocked() const override { return blocked_; }

	inline virtual FVector GetMoveDirection() const override { return moveDirection_; }
	inline virtual float GetMoveSpeed() const override { return moveSpeed_; }

	inline virtual CollisionRectangle GetCollisionRectangle() const override { return collisionRectangle_; }
	inline virtual CollisionRectangle GetPredictedFutureCollisionRectangle() const override { return futureCollisionRectangle_; }

	virtual void Visualize(float duration) const override;

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
	UPROPERTY(BluePrintReadWrite , EditAnywhere, Category = "Tram specific properties")
	TArray<USceneComponent*> carts_;

	// Accordions inbetween carts, set in BP constructor
	UPROPERTY(BluePrintReadWrite , EditAnywhere, Category = "Tram specific properties")
	TArray<USplineMeshComponent*> accordions_;

	// Draw tram debug shapes
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tram specific properties")
	bool drawDebug_ = false;

	// Default distance between neighbouring carts (length of inter-cart linkage)
	UPROPERTY(BluePrintReadWrite, EditAnywhere, Category = "Tram specific properties")
	float cartGap_ = 100.0f;
 
	// Distance between the wheelset of a cart at either end and the linkage joint
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tram specific properties")
	float frontRearCartWheelbase_ = 725.0f;
	
	// Distance between the wheelsets of a middle cart with multiple wheelsets
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tram specific properties")
	float middleCartWheelbase_ = 800.0f;

	// Offset of wheels of middle carts from the linkage joint
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tram specific properties")
	float middleCartWheelOffset_ = 200.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tram specific properties")
	FVector accordionStartPosition_ = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tram specific properties")
	FVector accordionStartTangent_ = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tram specific properties")
	FVector accordionEndPosition_ = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tram specific properties")
	FVector accordionEndTangent_ = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tram specific properties")
	float accordionOffset_ = 10.0f; 

	UFUNCTION(BlueprintNativeEvent)
	void BpOnFar();
	void BpOnFar_Implementation() {}

	UFUNCTION(BlueprintNativeEvent)
	void BpOnNear();
	void BpOnNear_Implementation() {}

	UFUNCTION(BlueprintNativeEvent)
	int BpGetVariantId() const;
	int BpGetVariantId_Implementation() { return 0; }

	UFUNCTION(BlueprintNativeEvent)
	void BpSetVariantId(int variantId);
	void BpSetVariantId_Implementation(int variantId) {}

	UFUNCTION(BlueprintNativeEvent)
	int BpNumVariants() const;
	int BpNumVariants_Implementation() { return 0; }

	bool simulate_ = false;
	bool useEditorTick_ = false;

	CurvePathFollower pathFollower_;
	FRandomStream* rng_;
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
	virtual void BeginDestroy() override;
	virtual void Destroyed() override;

	bool BlockedBy(ITrafficEntity* trafficEntity) const;
	bool BlockedBy(const FVector& point) const;
};
