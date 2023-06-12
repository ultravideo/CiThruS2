#include "SceneCapture360.h"
#include "../Debug.h"

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
	perspectiveSceneCaptureComponent_->SetRelativeRotation(CAMERA_ROTATIONS[5]);

	// Set this actor to call Tick() every frame
	PrimaryActorTick.bCanEverTick = true;
}

void ASceneCapture360::EnableCameras(const bool& use360Capture)
{
	if (use360Capture)
	{
		capture_360_ = true;
	}
	else
	{
		capture_perspective_ = true;
	}

	Debug::Log("cameras enabled");
}

void ASceneCapture360::DisableCameras()
{
	capture_360_ = false;
	capture_perspective_ = false;

	Debug::Log("cameras disabled");
}

void ASceneCapture360::SetRenderTargetResolution(const uint16_t& width, const uint16_t& height)
{
	for (USceneCaptureComponent2D* sceneCaptureComponent : sceneCaptureComponents_)
	{
		sceneCaptureComponent->TextureTarget->ResizeTarget(width, height);
	}

	perspectiveSceneCaptureComponent_->TextureTarget->ResizeTarget(width, height);
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

	DisableCameras();
}

void ASceneCapture360::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

	if (capture_360_)
	{
		for (USceneCaptureComponent2D* sceneCaptureComponent : sceneCaptureComponents_)
		{
			sceneCaptureComponent->CaptureSceneDeferred();
		}
	}

	if (capture_perspective_)
	{
		perspectiveSceneCaptureComponent_->CaptureSceneDeferred();
	}
}

bool ASceneCapture360::ShouldTickIfViewportsOnly() const
{
	return capture_360_ || capture_perspective_;
}
