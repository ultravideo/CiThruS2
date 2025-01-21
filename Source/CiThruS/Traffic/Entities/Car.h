#pragma once

#include "Traffic/TrafficController.h"
#include "Traffic/Paths/CurvePathFollower.h"
#include "Traffic/CollisionRectangle.h"
#include "ITrafficEntity.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Car.generated.h"

class ATrafficStopArea;
class APedestrian;
class AParkingSpace;

// "Driver" specific variables; Affect how they drive.
struct DriverCharacteristics {
	float maxSpeed = 1400; // Maximum speed this vehicle will ever go
	float normalSpeedMultiplier = 1.0f; //How much over the speed limit the vehicle tends to go

	// Set defaults randomly
	DriverCharacteristics()
	{
		float rand = FMath::RandRange(0.0f, 1.0f);
		maxSpeed = 600;
		//maxSpeed = 1246 * FMath::Pow(rand, 0.2141f); // ~25-44 km/h, should probably be more
		normalSpeedMultiplier = 1 + FMath::RandRange(0.0f, 0.2f); // 1-1.2 times faster than the speed limit
	}
};

USTRUCT(BlueprintType)
struct FCarVisualPart
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite)
	UStaticMesh* mesh = nullptr;
	UPROPERTY(BlueprintReadWrite)
	TArray<UMaterialInterface*> materials;
	UPROPERTY(BlueprintReadWrite)
	FTransform transform;
};

USTRUCT(BlueprintType)
struct FCarVisualVariant
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite)
	TArray<FCarVisualPart> parts;
	UPROPERTY(BlueprintReadWrite)
	int variantId = 0;
};

// A simulated car that follows roads and obeys traffic lights
UCLASS(Abstract)
class CITHRUS_API ACar : public AActor, public ITrafficEntity
{
	GENERATED_BODY()
	
public:	
	virtual void Tick(float deltaTime) override;

	virtual void Simulate(const KeypointGraph* graph);

	inline CurvePathFollower GetPathFollower() const { return pathFollower_; }
	inline ATrafficController* GetController() const { return trafficController_; }

	inline void SetMoveSpeed(float speed) { targetSpeed_ = FMath::Clamp(speed, -driverCharacteristics_.maxSpeed, driverCharacteristics_.maxSpeed); }
	inline void SetInstantSpeed(float speed) { moveSpeed_ = speed; }
	inline void ResetMoveSpeed() { SetMoveSpeed(trafficController_->GetRegulatedSpeedAtPoint(pathFollower_.GetLocation()) * driverCharacteristics_.normalSpeedMultiplier); }
	inline float GetMoveSpeed() const { return moveSpeed_; }
	inline float GetTargetSpeed() const { return targetSpeed_; }
	inline DriverCharacteristics GetDriverCharacteristics() const { return driverCharacteristics_; }

	void ApplyCustomPath(TArray<int> path, TArray<FVector> customPoints, int point, float progress);

	inline virtual void SetController(ATrafficController* controller) override { trafficController_ = controller; }

	inline virtual int32 GetKeypointRuleExceptions() const override { return keypointRuleExceptions_; }

	UFUNCTION(BlueprintNativeEvent)
	TArray<FCarVisualVariant> GetParkedVariants();
	TArray<FCarVisualVariant> GetParkedVariants_Implementation() { return {}; }

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	int GetVariantId();
	int GetVariantId_Implementation() { return 0; }

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void SetVariantId(int variantId);
	void SetVariantId_Implementation(int variantId) { }

	virtual void OnEnteredStopArea(ATrafficStopArea* stopArea) override;
	virtual void OnExitedStopArea(ATrafficStopArea* stopArea) override;

	virtual void OnEnteredFutureStopArea(ATrafficStopArea* stopArea) override;
	virtual void OnExitedFutureStopArea(ATrafficStopArea* stopArea) override;

	virtual void OnEnteredYieldArea(ATrafficYieldArea* yieldArea) override;
	virtual void OnExitedYieldArea(ATrafficYieldArea* yieldArea) override;

	virtual void OnFar() override;
	virtual void OnNear() override;

	virtual void UpdateBlockingCollisionWith(ITrafficEntity* otherEntity) override;
	virtual void UpdateBlockingCollisionWith(FVector position) override;
	virtual void UpdatePawnCollision(CollisionRectangle pawnCollision) override;

	virtual void ClearBlockingCollisions() override;

	inline virtual bool Yielding() const override { return shouldYield_; }
	inline virtual bool Stopped() const override { return blocked_ || inActiveStopArea_; }

	inline virtual FVector GetMoveDirection() const override { return moveDirection_; }
	inline float GetWheelbase() const { return wheelbase_; }

	inline virtual CollisionRectangle GetCollisionRectangle() const override { return collisionRectangle_; }
	inline virtual CollisionRectangle GetPredictedFutureCollisionRectangle() const override { return futureCollisionRectangle_; }

	inline virtual bool ShouldTickIfViewportsOnly() const override { return useEditorTick_; }

	// Animation stuff
	UFUNCTION(BlueprintCallable)
	float GetAnimWheelRotationSpeed() { return moveSpeed_ >= 50 ? (reverseUntilPoint == -1 ? moveSpeed_ / 100 : -moveSpeed_ / 100) : 0.0f; }
	UFUNCTION(BlueprintCallable)
	float GetAnimSteeringAngle();
	UFUNCTION(BlueprintCallable)
	float GetAccelerationSpeed() { return animAcceleration_; }
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vehicle Animations")
	bool enableAnimation = true;

	// Parking controls
	AParkingSpace* spaceBeingParkedTo_ = nullptr;
	int reverseUntilPoint = -1;
	int reverseFromPoint = -1;
	void ReverseUntilPoint(int point);
	void StartReverseFromPoint(int point);
	/* Wait until specified area is clear of other entities */
	void WaitForUnobstructedDepart(FVector collisionDimensions, FVector collisionPositionOffset);
	bool waitForUnobstructedDepart = false;
	
	// Debugging
	UFUNCTION(BlueprintCallable)
	TArray<FVector> DEBUG_GetPath();

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle properties")
	float targetSpeed_ = 500.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle properties")
	float moveSpeed_ = 500.0f;

	DriverCharacteristics driverCharacteristics_;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle properties")
	float wheelbase_ = 250.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC vehicle properties")
	FVector collisionDimensions_ = FVector(500.0f, 200.0f, 150.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Bitmask, BitmaskEnum = "/Script/CiThruS.EKeypointRules"), Category = "NPC vehicle properties")
	int32 keypointRuleExceptions_;

	bool simulate_ = false;
	bool useEditorTick_ = false;

	CurvePathFollower pathFollower_;
	CollisionRectangle collisionRectangle_;
	CollisionRectangle futureCollisionRectangle_;

	bool shouldYield_;
	bool inActiveStopArea_;
	bool inActiveFutureStopArea_;
	bool blocked_;
	bool blockedByFuturePawn_;

	TSet<ATrafficStopArea*> overlappedStopAreas_;
	TSet<ATrafficYieldArea*> overlappedYieldAreas_;

	TSet<ATrafficStopArea*> futureStopAreas_;

	ATrafficController* trafficController_;

	FVector moveDirection_;
	float animAcceleration_ = 0.0f;

	ACar();

	virtual void Move(const float& deltaTime);
	virtual void SetColliders();

	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	UFUNCTION(BlueprintNativeEvent)
	void Far();
	void Far_Implementation() { };

	UFUNCTION(BlueprintNativeEvent)
	void Near();
	void Near_Implementation() { };

	// Check if can overtake & do it
	bool Overtake();
	bool AllowedToOvertake(float& laneMinSpeed);
	bool GetOvertakeLanesAndParams(FVector& laneStart, FVector& laneEnd, FVector& secondLaneStart, FVector& secondLaneEnd, int& secondLaneEndIndex, float& laneLength, float& progressOnLane);
	bool CheckAdjacentOvertakeCollisions(float minSpeed, FVector laneCenter, FVector laneForward);
	void CreateOvertakePath(TArray<int>& path, TArray<FVector>& customPoints, int adjLaneEndIndex, FVector stayAtLanePoint, FVector mergeToPoint);

	// Settings for overtaking
	struct {
		bool allow;
		bool allowBothWays;
		float maxSpeedDiffMultiplier = 0.83f; // Minimum travel speed multiplier to initiate overtaking
		float maxSpeedDiffMultiplierBehind = 1.2f;// Maximum travel speed allowed for incoming cars on adjacent lane compared to own speed
		float minSpeedDiffMultiplierFront = 1.1f; // Minimum travel speed allowed for cars in front on adjacent lane compared to the one youre trying to pass
		float mergeDistance = 1650.0f; // What distance to merge to on adjacent lane
		float keepLaneDistance = 300.0f; // How long should stay on own lane before turning / should be more than or at least 0

		// Settings for collision check on adjacent lane
		float collisionCheckLengthBehind = 2000.0f;
		float collisionCheckOffsetBehind = (-500 - 1000.0f) - mergeDistance - keepLaneDistance;

		float collisionCheckLengthSide = 1000.0f;
		float collisionCheckOffsetSide = 0.0f - mergeDistance - keepLaneDistance;

		float collisionCheckLengthFront = 2500.0f;
		float collisionCheckOffsetFront = (500.0f + 1250.0f) - mergeDistance - keepLaneDistance;
	} overtakeSettings_;
	
	bool lowDetail_ = true;

	// Area collision check functions
	TArray<AActor*> GetEntitiesInArea(FVector center, FVector forward, float length);
	TArray<float> GetEntitiesSpeedInArea(FVector center, FVector forward, float length);
	TArray<float> GetEntitiesSpeedInFront(float length);

	// Sight/detection settings used for area collision checks
	const float widthOfAreaCheck_ = 220.0f; // Width of different checks for other cars (should be one lane width)
	const float frontalSightLength_ = 2500.0f; // How far detect other cars in front.
	const float allowedRotationDifferenceInFront = 8.0f; // Only allow cars to differ by this much (in degrees) when detecting cars in front => reduce bad cases in curves
	
	// Used to limit how often certain stuff runs in Tick()
	int frameCounter_;

	bool BlockedBy(ITrafficEntity* trafficEntity) const;
	bool BlockedBy(const FVector& point) const;
};
