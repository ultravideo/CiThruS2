#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <iostream>
#include <memory>
#include <fstream>
#include <thread>
#include <mutex>
#include <vector>
#include <thread>
#include <array>

#include "PerformanceMeasurer.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class RenderTargetReader;
class Pipeline;
class AsyncPipelineRunner;

const int MAX_CAMERAS = 10;

UCLASS()
class APerformanceMeasurer : public AActor
{
	GENERATED_BODY()
	
public:	
	APerformanceMeasurer();

	virtual void Tick(float deltaTime) override;

	inline virtual bool ShouldTickIfViewportsOnly() const override { return useEditorTick_; }

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Controls")
	void StartTransmit();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Controls")
	void StopTransmit();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Controls")
	void ResetValues();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stream Debug")
	void ResetStreams();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	FString remoteStreamIp_ = "127.0.0.1";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remoteStreamPort_ = 12300;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int cameraCount_ = 4;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remoteStreamWidth_ = 1024;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General Stream Settings")
	int remoteStreamHeight_ = 1024;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Kvazaar Settings")
	int overlappedWavefront_ = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Kvazaar Settings")
	int wavefrontParallelProcessing_ = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Kvazaar Settings")
	int quantizationParameter_ = 27;

	UPROPERTY(BlueprintReadOnly)
	float minFps_;

	UPROPERTY(BlueprintReadOnly)
	float maxFps_;

	UPROPERTY(BlueprintReadOnly)
	float meanFps_;

protected:
	bool transmitEnabled_ = false;
	bool useEditorTick_ = true;

	std::mutex streamMutex_;

	TArray<USceneCaptureComponent2D*> cameras_ = {};

	UPROPERTY()
	TArray<TObjectPtr<UTextureRenderTarget2D>> renderTargets_ = {};

	std::array<RenderTargetReader*, MAX_CAMERAS> readers_;
	std::array<AsyncPipelineRunner*, MAX_CAMERAS> runners_;

	int activeCameras_;

	bool wantsStop_ = false;

	float minFpsInternal_ = 10000000.0f;
	float maxFpsInternal_ = 0.0f;
	float fpsPeriod_ = 0.0f;
	int frameCount_ = 0;

	virtual void PostRegisterAllComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	void DeleteStreams();

	void StartStreams();

	void StopTransmitInternal();
};
