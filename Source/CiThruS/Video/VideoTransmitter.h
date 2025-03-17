#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TextureRenderTarget.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Engine.h"
#include "RHI.h"
#include "RenderResource.h"
#include "SceneCapture360.h"

#include <iostream>
#include <memory>
#include <fstream>
#include <thread>
#include <mutex>

#include "VideoTransmitter.generated.h"

class RenderTargetReader;
class IImageFilter;
class ImagePipeline;
class SyncBarrier;
class AnnotationTransmitter;
class HevcEncoder;
class RtpTransmitter;

// Transmits 360 or regular video through an RTP stream
UCLASS()
class CITHRUS_API AVideoTransmitter : public AActor
{
	GENERATED_BODY()
	
public:	
	AVideoTransmitter();

	virtual void Tick(float deltaTime) override;

	inline virtual bool ShouldTickIfViewportsOnly() const override { return useEditorTick_; }

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Controls")
	void StartTransmit();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Controls")
	void StopTransmit();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	FString remoteStreamIp_ = "127.0.0.1";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remoteVideoDstPort_ = 8888;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remoteStreamWidth_ = 1280;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remoteStreamHeight_ = 720;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	ASceneCapture360* sceneCapture_;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int processingThreadCount_ = 8;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	bool transmitAsynchronously_ = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Kvazaar Settings")
	int overlappedWavefront_ = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Kvazaar Settings")
	int wavefrontParallelProcessing_ = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Kvazaar Settings")
	int quantizationParameter_ = 27;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Annotation Settings")
	int remoteAnnotationsDstPort_ = 9999;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Annotation Settings")
	bool sendAnnotations_ = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Annotation Settings")
	bool drawBoundingBoxes_ = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "360 Stream Settings")
	bool enable360Capture_ = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "360 Stream Settings")
	int widthAndHeightPerCaptureSide_ = 512;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "360 Stream Settings")
	bool bilinearFiltering_ = true;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Debug")
	void ResetStreams();

private:
	RenderTargetReader* reader_;
	ImagePipeline* pipeline_;
	AnnotationTransmitter* annotationTransmitter_;

	uint16_t transmitFrameWidth_;
	uint16_t transmitFrameHeight_;

	std::mutex streamCreationMutex_;
	std::thread encodingThread_;

	std::list<MarkerCaptureData> markerData_;

	int frameNumber_;

	int threadCount_;

	bool terminateEncodingThread_;

	bool transmit360Video_;
	bool transmitAsync_;
	bool transmitEnabled_;

	bool useEditorTick_;

	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	void DeleteStreams();

	void StartStreams();

	void CaptureAndTransmit();

	void EncodeAndTransmitFrame();

	void TransmitAsync();
};
