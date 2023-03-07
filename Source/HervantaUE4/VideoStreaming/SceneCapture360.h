#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h" 
#include "SceneCapture360.generated.h"


UCLASS()
class HERVANTAUE4_API ASceneCapture360 : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASceneCapture360();

	void EnableCameras(const bool& use360Capture);
	void DisableCameras();

	void SetRenderTargetResolution(const uint16_t& width, const uint16_t& height);

	int Get360CaptureTargetCount() const;
	UTextureRenderTarget2D* GetPerspectiveFrameTarget() const;
	UTextureRenderTarget2D* Get360FrameTarget(const int& index) const;

protected:
	TArray<USceneCaptureComponent2D*> sceneCaptureComponents_;
	USceneCaptureComponent2D* perspectiveSceneCaptureComponent_;

	bool capture_perspective_;
	bool capture_360_;

	virtual void PostRegisterAllComponents() override;
	virtual void Tick(float deltaTime) override;
	virtual bool ShouldTickIfViewportsOnly() const override;
};
