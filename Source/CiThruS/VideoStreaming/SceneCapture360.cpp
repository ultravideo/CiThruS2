#include "SceneCapture360.h"
#include "Misc/Debug.h"

const FRotator CAMERA_ROTATIONS[] =
{
	FRotator(90.0f, 0.0f, 0.0f),
	FRotator(0.0f, -90.0f, 0.0f),
	FRotator(0.0f, 0.0f, 0.0f),
	FRotator(0.0f, 90.0f, 0.0f),
	FRotator(0.0f, 180.0f, 0.0f),
	FRotator(-90.0f, 0.0f, 0.0f)
};

// Six faces in a cube and each capture target corresponds to one face
const int CAPTURE_TARGET_COUNT_360 = 6;

const uint16_t RENDER_TARGET_DEFAULT_RESOLUTION = 512;

ASceneCapture360::ASceneCapture360()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	for (int i = 0; i < CAPTURE_TARGET_COUNT_360; i++)
	{
		USceneCaptureComponent2D* sceneCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(FName("360SceneCaptureComponent" + FString::FromInt(i)));
		sceneCaptureComponent->SetupAttachment(RootComponent);
		sceneCaptureComponent->SetRelativeRotation(CAMERA_ROTATIONS[i]);

		sceneCaptureComponents_.Add(sceneCaptureComponent);
	}

	perspectiveSceneCaptureComponent_ = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PerspectiveSceneCaptureComponent"));
	perspectiveSceneCaptureComponent_->SetupAttachment(RootComponent);

	// Set this actor to call Tick() every frame
	PrimaryActorTick.bCanEverTick = true;
}

void ASceneCapture360::EnableCameras(const bool& use360Capture)
{
	if (use360Capture)
	{
		capture360_ = true;
	}
	else
	{
		capturePerspective_ = true;
	}

	Debug::Log("cameras enabled");
}

void ASceneCapture360::DisableCameras()
{
	capture360_ = false;
	capturePerspective_ = false;

	std::lock_guard<std::mutex> lock(markerDataMutex_);

	markerData_.clear();

	Debug::Log("cameras disabled");
}

void ASceneCapture360::SetRenderTargetResolution(const uint16_t& width, const uint16_t& height)
{
	for (USceneCaptureComponent2D* sceneCaptureComponent : sceneCaptureComponents_)
	{
		sceneCaptureComponent->TextureTarget->ResizeTarget(width, height);
	}

	perspectiveSceneCaptureComponent_->TextureTarget->ResizeTarget(width, height);

	resolutionWidth_ = width;
	resolutionHeight_ = height;
}

int ASceneCapture360::Get360CaptureTargetCount() const
{
	return CAPTURE_TARGET_COUNT_360;
}

UTextureRenderTarget2D* ASceneCapture360::GetPerspectiveFrameTarget() const
{
	return perspectiveSceneCaptureComponent_->TextureTarget;
}

UTextureRenderTarget2D* ASceneCapture360::Get360FrameTarget(const int& index) const
{
	return sceneCaptureComponents_[index]->TextureTarget;
}

std::list<MarkerCaptureData> ASceneCapture360::GetMarkerData()
{
	std::lock_guard<std::mutex> lock(markerDataMutex_);

	return markerData_;
}

void ASceneCapture360::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	for (USceneCaptureComponent2D* sceneCaptureComponent : sceneCaptureComponents_)
	{
		if (sceneCaptureComponent->TextureTarget == nullptr)
		{
			sceneCaptureComponent->TextureTarget = NewObject<UTextureRenderTarget2D>();
			sceneCaptureComponent->TextureTarget->InitCustomFormat(RENDER_TARGET_DEFAULT_RESOLUTION, RENDER_TARGET_DEFAULT_RESOLUTION, PF_B8G8R8A8, false);
			sceneCaptureComponent->TextureTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
		}

		sceneCaptureComponent->bCaptureEveryFrame = false;
		sceneCaptureComponent->bCaptureOnMovement = false;
	}

	if (perspectiveSceneCaptureComponent_->TextureTarget == nullptr)
	{
		perspectiveSceneCaptureComponent_->TextureTarget = NewObject<UTextureRenderTarget2D>();
		perspectiveSceneCaptureComponent_->TextureTarget->InitCustomFormat(RENDER_TARGET_DEFAULT_RESOLUTION, RENDER_TARGET_DEFAULT_RESOLUTION, PF_B8G8R8A8, false);
		perspectiveSceneCaptureComponent_->TextureTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
	}

	perspectiveSceneCaptureComponent_->bCaptureEveryFrame = false;
	perspectiveSceneCaptureComponent_->bCaptureOnMovement = false;

	capture360_ = false;
	capturePerspective_ = false;
}

void ASceneCapture360::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

	if (capture360_)
	{
		for (USceneCaptureComponent2D* sceneCaptureComponent : sceneCaptureComponents_)
		{
			sceneCaptureComponent->CaptureSceneDeferred();
		}
	}

	if (capturePerspective_)
	{
		perspectiveSceneCaptureComponent_->CaptureScene();
	}

	if (markerGroupToCapture_)
	{
		std::lock_guard<std::mutex> lock(markerDataMutex_);

		markerData_.clear();

		for (const FPoseEstimationMarker& marker : markerGroupToCapture_->markers_)
		{
			FVector corners[4] =
			{
				FVector(-marker.widthAndHeight * 0.5f, -marker.widthAndHeight * 0.5f, 0.0f) + marker.relativeCenter,
				FVector(-marker.widthAndHeight * 0.5f, marker.widthAndHeight * 0.5f, 0.0f) + marker.relativeCenter,
				FVector(marker.widthAndHeight * 0.5f, marker.widthAndHeight * 0.5f, 0.0f) + marker.relativeCenter,
				FVector(marker.widthAndHeight * 0.5f, -marker.widthAndHeight * 0.5f, 0.0f) + marker.relativeCenter
			};

			FVector2D corners2D[4] =
			{
				FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector
			};

			float aspectRatioInverse = static_cast<float>(resolutionHeight_) / resolutionWidth_;
			FMatrix viewMatrix = GetActorTransform().Inverse().ToMatrixNoScale();

			float s = 1.0f / tan(FMath::DegreesToRadians(perspectiveSceneCaptureComponent_->FOVAngle / 2.0f));

			FVector2D bboxMin;
			FVector2D bboxMax;

			bool isBehindCamera = true;

			for (int i = 0; i < 4; i++)
			{
				FVector posInWorldSpace = markerGroupToCapture_->GetActorLocation() + markerGroupToCapture_->GetActorRotation().RotateVector(corners[i] * markerGroupToCapture_->GetActorScale() * 100.0f);
				FVector4 posInCameraSpace = viewMatrix.TransformPosition(posInWorldSpace);
				FVector2D posProjected = FVector2D(-posInCameraSpace.Y * s / (posInCameraSpace.Z * 2.0f) + 0.5f, posInCameraSpace.X * s / (posInCameraSpace.Z * 2.0f * aspectRatioInverse) + 0.5f);

				if (posInCameraSpace.Z < 0)
				{
					isBehindCamera = false;
				}

				corners[i] = FVector(posInCameraSpace.Y / 100.0f, posInCameraSpace.X / 100.0f, posInCameraSpace.Z / 100.0f);
				corners2D[i] = posProjected;

				if (i == 0)
				{
					bboxMin = posProjected;
					bboxMax = posProjected;
				}
				else
				{
					if (posProjected.X < bboxMin.X)
					{
						bboxMin.X = posProjected.X;
					}

					if (posProjected.Y < bboxMin.Y)
					{
						bboxMin.Y = posProjected.Y;
					}

					if (posProjected.X > bboxMax.X)
					{
						bboxMax.X = posProjected.X;
					}

					if (posProjected.Y > bboxMax.Y)
					{
						bboxMax.Y = posProjected.Y;
					}
				}
			}

			if (!isBehindCamera && bboxMin.X < 1.0f && bboxMin.Y < 1.0f && bboxMax.X > 0.0f && bboxMax.Y > 0.0f)
			{
				markerData_.push_back(
					{
						marker.arucoIndex,
						{ corners[0], corners[1], corners[2], corners[3] },
						{ corners2D[0], corners2D[1], corners2D[2], corners2D[3] },
						bboxMin,
						bboxMax
					});
			}
		}
	}
}

bool ASceneCapture360::ShouldTickIfViewportsOnly() const
{
	return capture360_ || capturePerspective_;
}
