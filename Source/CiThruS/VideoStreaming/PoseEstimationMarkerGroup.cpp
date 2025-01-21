#include "PoseEstimationMarkerGroup.h"
#include "Misc/Debug.h"

APoseEstimationMarkerGroup::APoseEstimationMarkerGroup()
{
	visualMesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	visualMesh_->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> visualAsset(TEXT("/Engine/BasicShapes/Plane"));

	if (visualAsset.Succeeded())
	{
		visualMesh_->SetStaticMesh(visualAsset.Object);
	}
}

void APoseEstimationMarkerGroup::PostInitProperties()
{
	Super::PostInitProperties();

	visualMesh_->SetMaterial(0, material_);
}

void APoseEstimationMarkerGroup::Visualize()
{
	for (const FPoseEstimationMarker& marker : markers_)
	{
		FVector corners[4] =
		{
			FVector(-marker.widthAndHeight * 0.5f, -marker.widthAndHeight * 0.5f, 0.0f) + marker.relativeCenter,
			FVector(-marker.widthAndHeight * 0.5f, marker.widthAndHeight * 0.5f, 0.0f) + marker.relativeCenter,
			FVector(marker.widthAndHeight * 0.5f, marker.widthAndHeight * 0.5f, 0.0f) + marker.relativeCenter,
			FVector(marker.widthAndHeight * 0.5f, -marker.widthAndHeight * 0.5f, 0.0f) + marker.relativeCenter
		};

		for (int i = 0; i < 4; i++)
		{
			FVector posInWorldSpace = GetActorLocation() + GetActorRotation().RotateVector(corners[i] * GetActorScale() * 100.0f);

			Debug::DrawTemporaryLine(GetWorld(), posInWorldSpace, posInWorldSpace + GetActorUpVector() * 10.0f, FColor::Blue, 5.0f, 1.0f);
		}
	}
}
