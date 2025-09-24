#include "RoadRegulationZone.h"
#include "UObject/ConstructorHelpers.h"
#include "Traffic/Entities/ITrafficEntity.h"
#include "Misc/Debug.h"
#include "Traffic/Entities/Car.h"

ARoadRegulationZone::ARoadRegulationZone()
{
    PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITOR
    SetIsSpatiallyLoaded(false);
#endif

    SetActorHiddenInGame(true);

    // Create the static mesh component
    mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    RootComponent = mesh_;
    mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Set the default static mesh
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (MeshAsset.Succeeded())
    {
        mesh_->SetStaticMesh(MeshAsset.Object);
    }
}

void ARoadRegulationZone::BeginPlay()
{
    Super::BeginPlay();

    collisionRectangle_ = CollisionRectangle(GetActorScale() * 100.0f, GetActorLocation() + FVector::UpVector * GetActorScale().Z * 50.0f, GetActorRotation().Quaternion());
}

void ARoadRegulationZone::OnConstruction(const FTransform& Transform)
{
    mesh_->SetVisibility(false);
    SetMeshColor();
}

void ARoadRegulationZone::SetMeshColor()
{
    // Set color of material
    UMaterial* material = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_VehicleRuleZone.M_VehicleRuleZone"), nullptr, LOAD_None, nullptr);
    UMaterialInstanceDynamic* dynamicMaterial;
    dynamicMaterial = UMaterialInstanceDynamic::Create(material, nullptr);

    if (dynamicMaterial)
    {
        dynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), zoneRules.color); // Set color parameter
    }

    mesh_->SetMaterial(0, dynamicMaterial);
}

void ARoadRegulationZone::UpdateCollisionStatusWithEntity(ITrafficEntity* entity)
{
    if (ACar* car = Cast<ACar>(entity))
    {
        bool isIntersecting = collisionRectangle_.IsIntersecting(entity->GetCollisionRectangle());
        // Check if entity is overlapping this stop area
        if (isIntersecting)
        {
            bool actorWasAlreadyInSet = false;

            overlappingEntities_.Add(entity, &actorWasAlreadyInSet);

            // Notify the entity if it was not previously in this stop area
            if (!actorWasAlreadyInSet)
            {
                car->SetMoveSpeed(zoneRules.speedLimit * car->GetDriverCharacteristics().normalSpeedMultiplier);
            }
        }
        else
        {
            // Notify the entity if it was previously in this stop area
            if (overlappingEntities_.Remove(entity) != 0)
            {
                // Set back to default speed limit
                car->ResetMoveSpeed();
            }
        }
    }
}

void ARoadRegulationZone::ApplySettings()
{
    SetMeshColor();
}
