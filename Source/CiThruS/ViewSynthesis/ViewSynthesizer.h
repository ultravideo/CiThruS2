#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <iostream>
#include <memory>
#include <fstream>
#include <thread>
#include <mutex>
#include <vector>

#include "ViewSynthesizer.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class RenderTargetReaderWithUserData;
class RenderTargetWriter;
class AsyncPipelineRunner;

// Performs view synthesis
UCLASS()
class AViewSynthesizer : public AActor
{
	GENERATED_BODY()
	
public:	
	AViewSynthesizer();

	virtual void Tick(float deltaTime) override;

	inline virtual bool ShouldTickIfViewportsOnly() const override { return useEditorTick_; }

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Controls")
	void StartTransmit();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Controls")
	void StopTransmit();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	FString remoteStreamIp_ = "127.0.0.1";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remoteStreamPort_ = 12300;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remoteStreamWidth_ = 1024;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remoteStreamHeight_ = 1024;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	float frontFov_ = 90.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	float rearFov_ = 60.0f;

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Depth Settings")
	float depthRange_ = 150.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Depth Settings")
	TObjectPtr<UTextureRenderTarget2D> resultRenderTarget_ = nullptr;

protected:
	struct ViewSynthCameraParams
	{
		uint32_t frameNumber;
		FVector3f pos;
		FVector3f rot;
		float fov;
		float depthRange;
		uint64_t timestamp;
	};

	bool transmitEnabled_ = false;
	bool useEditorTick_ = true;

	std::mutex streamMutex_;

	USceneCaptureComponent2D* frontCamera_ = nullptr;
	USceneCaptureComponent2D* rearCamera_ = nullptr;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> frontRenderTarget_ = nullptr;
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> rearRenderTarget_ = nullptr;

	RenderTargetReaderWithUserData* frontReader_;
	RenderTargetReaderWithUserData* rearReader_;

	RenderTargetWriter* resultWriter_;

	std::vector<AsyncPipelineRunner*> runners_;

	uint32_t frameNumber_;
	uint64_t startTimestampMs_;

	bool wantsStop_ = false;

	virtual void PostRegisterAllComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	bool StartStreams();
	void DeleteStreams();
	bool ResetStreams();

	void StopTransmitInternal();
	void Capture();
};
