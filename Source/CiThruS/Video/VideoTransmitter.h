#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TextureRenderTarget.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Engine.h"
#include "RHI.h"
#include "RenderResource.h"

#include <iostream>
#include <memory>
#include <fstream>
#include <thread>
#include <mutex>

#include "VideoTransmitter.generated.h"

class USceneCaptureComponent2D;
class RenderTargetReader;
class AsyncPipelineRunner;

// Transmits 360 or regular video through an RTP stream
UCLASS()
class CITHRUS_API AVideoTransmitter : public AActor
{
	GENERATED_BODY()
	
public:	
	AVideoTransmitter();

	inline virtual bool ShouldTickIfViewportsOnly() const override { return useEditorTick_; }

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Controls")
	void StartTransmit();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Controls")
	void StopTransmit();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	FString remoteStreamIp_ = "127.0.0.1";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remoteVideoDstPort_ = 12300;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remoteStreamWidth_ = 1280;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remoteStreamHeight_ = 720;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Depth Settings")
	float fov_ = 60.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int processingThreadCount_ = 8;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	bool saveToFile_ = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	FString saveDirectory_ = FString(FPlatformProcess::UserDir()) + "CiThruS2/Recorded/";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Kvazaar Settings")
	int overlappedWavefront_ = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Kvazaar Settings")
	int wavefrontParallelProcessing_ = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Kvazaar Settings")
	int quantizationParameter_ = 27;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "360 Stream Settings")
	bool enable360Capture_ = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "360 Stream Settings")
	int widthAndHeightPerCaptureSide_ = 512;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "360 Stream Settings")
	bool bilinearFiltering_ = true;

private:
	TArray<USceneCaptureComponent2D*> cubemapCameras_;
	USceneCaptureComponent2D* normalCamera_;

	AsyncPipelineRunner* runner_;
	RenderTargetReader* reader_;

	std::mutex streamMutex_;

	bool wantsStop_;

	bool transmitEnabled_;
	bool capture360_;

	bool useEditorTick_;

	virtual void PostRegisterAllComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;
	virtual void Tick(float deltaTime) override;

	bool StartStreams();
	void DeleteStreams();
	bool ResetStreams();

	void StopTransmitInternal();
};
