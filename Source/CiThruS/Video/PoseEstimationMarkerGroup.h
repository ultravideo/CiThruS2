#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "PoseEstimationMarkerGroup.generated.h"

struct MarkerCaptureData
{
	int id;
	FVector cornerPositions[4];
	FVector2D cornerPositions2D[4];
	FVector2D boundingBoxMin;
	FVector2D boundingBoxMax;
};

USTRUCT()
struct FPoseEstimationMarker
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FVector relativeCenter = FVector(0, 0, 0);

	UPROPERTY(EditAnywhere)
	float widthAndHeight = 0.0f;

	UPROPERTY(EditAnywhere)
	int arucoIndex = 0;
};

// Marker for use with pose estimation
UCLASS()
class CITHRUS_API APoseEstimationMarkerGroup : public AActor
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere)
	UMaterial* material_;

	UPROPERTY(EditAnywhere)
	TArray<FPoseEstimationMarker> markers_;

	APoseEstimationMarkerGroup();

	virtual void PostInitProperties() override;

	UFUNCTION(CallInEditor)
	void Visualize();

protected:
	UStaticMeshComponent* visualMesh_;
};
