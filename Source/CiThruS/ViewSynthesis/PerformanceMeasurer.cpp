#include "PerformanceMeasurer.h"

#include "Pipeline/Pipeline.h"
#include "Pipeline/AsyncPipelineRunner.h"
#include "Pipeline/Components/RenderTargetReader.h"
#include "Pipeline/Components/RgbaToYuvConverter.h"
#include "Pipeline/Components/HevcEncoder.h"
#include "Pipeline/Components/RtpTransmitter.h"
#include "Pipeline/Scaffolding/SequentialFilter.h"

#include "Misc/Debug.h"

#include "RHI.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "ShaderParameterStruct.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderResource.h"

#include <algorithm>

APerformanceMeasurer::APerformanceMeasurer()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

    cameras_.SetNumUninitialized(MAX_CAMERAS);

    for (int i = 0; i < MAX_CAMERAS; i++)
    {
        cameras_[i] = CreateDefaultSubobject<USceneCaptureComponent2D>(FName(FString("Camera") + FString::FromInt(i)));
        cameras_[i]->SetupAttachment(RootComponent);
    }

    // Set this actor to call Tick() every frame
    PrimaryActorTick.bCanEverTick = true;
    //PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void APerformanceMeasurer::PostRegisterAllComponents()
{
    Super::PostRegisterAllComponents();

    renderTargets_.SetNumUninitialized(MAX_CAMERAS);

    for (int i = 0; i < MAX_CAMERAS; i++)
    {
        renderTargets_[i] = NewObject<UTextureRenderTarget2D>();
        renderTargets_[i]->InitCustomFormat(512, 512, PF_A32B32G32R32F, false);
        renderTargets_[i]->RenderTargetFormat = RTF_RGBA32f;

        cameras_[i]->TextureTarget = renderTargets_[i];
        cameras_[i]->bCaptureEveryFrame = false;
        cameras_[i]->bCaptureOnMovement = false;
    }
}

void APerformanceMeasurer::EndPlay(const EEndPlayReason::Type endPlayReason)
{
    Super::EndPlay(endPlayReason);

	DeleteStreams();
}

void APerformanceMeasurer::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

    frameCount_++;
    fpsPeriod_ += deltaTime;

    float fps = 1.0f / deltaTime;

    if (fps < minFpsInternal_)
    {
        minFpsInternal_ = fps;
    }

    if (fps > maxFpsInternal_)
    {
        maxFpsInternal_ = fps;
    }

    minFps_ = minFpsInternal_;
    maxFps_ = maxFpsInternal_;
    meanFps_ = frameCount_ / fpsPeriod_;

    if (wantsStop_)
    {
        StopTransmitInternal();
    }

    const std::lock_guard<std::mutex> lock(streamMutex_);

    if (!transmitEnabled_)
    {
        return;
    }

    for (int i = 0; i < activeCameras_; i++)
    {
        cameras_[i]->CaptureScene();
    }

    for (int i = 0; i < activeCameras_; i++)
    {
        readers_[i]->Read();
    }
}

void APerformanceMeasurer::StartTransmit()
{
    const std::lock_guard<std::mutex> lock(streamMutex_);

    ResetStreams();

    transmitEnabled_ = true;
    useEditorTick_ = true;
}

void APerformanceMeasurer::StopTransmit()
{
    // Stop the transmit in a synchronized manner to avoid race conditions
    wantsStop_ = true;
}

void APerformanceMeasurer::StartStreams()
{
    activeCameras_ = std::min(cameraCount_, MAX_CAMERAS);

    for (int i = 0; i < activeCameras_; i++)
    {
        renderTargets_[i]->ResizeTarget(remoteStreamWidth_, remoteStreamHeight_);
        readers_[i] = new RenderTargetReader({ renderTargets_[i] }, true);

        runners_[i] = new AsyncPipelineRunner(
            new Pipeline(
                readers_[i],
                new RgbaToYuvConverter(remoteStreamWidth_, remoteStreamHeight_),
                new HevcEncoder(remoteStreamWidth_, remoteStreamHeight_, 16,
                    quantizationParameter_, wavefrontParallelProcessing_, overlappedWavefront_),
                new RtpTransmitter(TCHAR_TO_UTF8(*remoteStreamIp_), remoteStreamPort_ + i)));
    }
}

void APerformanceMeasurer::DeleteStreams()
{
    for (int i = 0; i < activeCameras_; i++)
    {
        delete runners_[i];
        runners_[i] = nullptr;

        // This is already deleted by the pipeline so don't delete it twice
        readers_[i] = nullptr;
    }
}

void APerformanceMeasurer::ResetStreams()
{
	DeleteStreams();
	StartStreams();
}

void APerformanceMeasurer::ResetValues()
{
    minFpsInternal_ = 10000000.0f;
    maxFpsInternal_ = 0.0f;
    meanFps_ = 0.0f;
    fpsPeriod_ = 0.0f;
    frameCount_ = 0;
}

void APerformanceMeasurer::StopTransmitInternal()
{
    const std::lock_guard<std::mutex> lock(streamMutex_);

    DeleteStreams();

    transmitEnabled_ = false;
    useEditorTick_ = false;
    wantsStop_ = false;
}
