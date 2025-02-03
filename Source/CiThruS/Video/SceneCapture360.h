#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h" 
#include "PoseEstimationMarkerGroup.h"

#include <list>
#include <queue>
#include <mutex>

#include "SceneCapture360.generated.h"

// Captures 360 video from the scene
UCLASS()
class CITHRUS_API ASceneCapture360 : public AActor
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere)
	APoseEstimationMarkerGroup* markerGroupToCapture_;

	// Sets default values for this actor's properties
	ASceneCapture360();

	void EnableCameras(const bool& use360Capture);
	void DisableCameras();

	void SetRenderTargetResolution(const uint16_t& width, const uint16_t& height);

	int Get360CaptureTargetCount() const;
	UTextureRenderTarget2D* GetPerspectiveFrameTarget() const;
	UTextureRenderTarget2D* Get360FrameTarget(const int& index) const;

	std::list<MarkerCaptureData> GetMarkerData();

protected:
	TArray<USceneCaptureComponent2D*> sceneCaptureComponents_;
	USceneCaptureComponent2D* perspectiveSceneCaptureComponent_;

	bool capturePerspective_;
	bool capture360_;

	uint16_t resolutionWidth_;
	uint16_t resolutionHeight_;

	std::list<MarkerCaptureData> markerData_;
	std::mutex markerDataMutex_;

	virtual void PostRegisterAllComponents() override;
	virtual void Tick(float deltaTime) override;
	virtual bool ShouldTickIfViewportsOnly() const override;
};
