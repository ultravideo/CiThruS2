#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ITrafficArea.h"
#include "Traffic/Paths/KeypointGraph.h"
#include "RoadRegulationZone.generated.h"

USTRUCT(BlueprintType)
struct FZoneRules
{
    GENERATED_BODY()
public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor color = FLinearColor::Red;

    /* Lower number takes priority if multiple zones overlap. Equal numbers apply both.*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int priority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float speedLimit = 833.0f; // ~ 30 km/h

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Bitmask, BitmaskEnum = "/Script/CiThruS.EKeypointRules"))
    int32 rules = 1;

    // + Operator is used to merge rules together, decide what takes priority
    FZoneRules operator+(const FZoneRules& other) const
    {
        FZoneRules res;
        // Priority higher, scrap other
        if (priority < other.priority)
        {
            res.color = color;
            res.priority = priority;
            res.speedLimit = speedLimit;
            res.rules = rules;

        }
        // Priority same, choose how to merge values
        else if (priority == other.priority)
        {
            res.color = color;
            res.priority = priority;
            res.speedLimit = speedLimit < other.speedLimit ? other.speedLimit : speedLimit; // Prefer higher speed limit
            // Rules can be added together
            res.rules = rules | other.rules;

        }
        // Priority higher, scrap other
        else if (priority > other.priority)
        {
            res.color = other.color;
            res.priority = other.priority;
            res.speedLimit = other.speedLimit;
            res.rules = other.rules;
        }
        return res;
    }
};

// A zone where traffic entities should continuously obey rules such as speed limits
UCLASS()
class CITHRUS_API ARoadRegulationZone : public AActor, public ITrafficArea
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    ARoadRegulationZone();

    virtual void UpdateCollisionStatusWithEntity(ITrafficEntity* entity);
    inline virtual CollisionRectangle GetCollisionRectangle() const { return collisionRectangle_; }
    inline float GetSpeedLimit() const { return zoneRules.speedLimit; }
    inline FZoneRules GetAllRules() const { return zoneRules; }

    UFUNCTION(CallInEditor, Category="Zone Settings")
    void ApplySettings();

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditAnywhere, Category = "Zone Settings")
    FZoneRules zoneRules;

    CollisionRectangle collisionRectangle_;
    TSet<ITrafficEntity*> overlappingEntities_;

    class UStaticMeshComponent* mesh_;

public:

    void SetMeshColor();

    // Used when zone collisions are needed inside editor (sim not running)
    inline void EDITOR_InitCollisions() { collisionRectangle_ = CollisionRectangle(GetActorScale() * 100.0f, GetActorLocation() + FVector::UpVector * GetActorScale().Z * 50.0f, GetActorRotation().Quaternion()); };
};
