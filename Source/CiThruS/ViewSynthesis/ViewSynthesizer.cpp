#include "ViewSynthesizer.h"

#include "Pipeline/Pipeline.h"
#include "Pipeline/AsyncPipelineRunner.h"

#include "Pipeline/Components/RenderTargetReaderWithUserData.h"
#include "Pipeline/Components/RenderTargetWriter.h"
#include "Pipeline/Components/FloatToByteConverter.h"
#include "Pipeline/Components/RgbaToYuvConverter.h"
#include "Pipeline/Components/YuvToRgbaConverter.h"
#include "Pipeline/Components/DepthToYuvConverter.h"
#include "Pipeline/Components/DepthSeparator.h"
#include "Pipeline/Components/ImageConcatenator.h"
#include "Pipeline/Components/SeiEmbedder.h"
#include "Pipeline/Components/CsvLogger.h"
#include "Pipeline/Components/HevcEncoder.h"
#include "Pipeline/Components/HevcDecoder.h"
#include "Pipeline/Components/SolidColorImageGenerator.h"
#include "Pipeline/Components/RtpTransmitter.h"
#include "Pipeline/Components/RtpReceiver.h"
#include "Pipeline/Components/BlinkerSource.h"
#include "Pipeline/Components/BlinkDetector.h"
#include "Pipeline/Components/PngRecorder.h"
#include "Pipeline/Components/FileSink.h"

#include "Pipeline/Scaffolding/PassthroughFilter.h"
#include "Pipeline/Scaffolding/DuplicatorFilter.h"
#include "Pipeline/Scaffolding/SidechainSource.h"
#include "Pipeline/Scaffolding/SequentialFilter.h"
#include "Pipeline/Scaffolding/SequentialSink.h"
#include "Pipeline/Scaffolding/ParallelFilter.h"
#include "Pipeline/Scaffolding/ParallelSource.h"
#include "Pipeline/Scaffolding/ParallelSink.h"

#include "Misc/Debug.h"

#include "RHI.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "ShaderParameterStruct.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderResource.h"

#include <algorithm>

AViewSynthesizer::AViewSynthesizer()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

    frontCamera_ = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("FrontCamera"));
    frontCamera_->SetupAttachment(RootComponent);
    frontCamera_->SetRelativeLocation(FVector(200.0f, 0.0f, 0.0f));
    frontCamera_->CaptureSource = ESceneCaptureSource::SCS_SceneColorSceneDepth;

    rearCamera_ = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("RearCamera"));
    rearCamera_->SetupAttachment(RootComponent);
    rearCamera_->CaptureSource = ESceneCaptureSource::SCS_SceneColorSceneDepth;

    // Set this actor to call Tick() every frame
    PrimaryActorTick.bCanEverTick = true;
    //PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void AViewSynthesizer::PostRegisterAllComponents()
{
    Super::PostRegisterAllComponents();

    static const uint16_t RENDER_TARGET_DEFAULT_RESOLUTION = 512;

    frontRenderTarget_ = NewObject<UTextureRenderTarget2D>();
    frontRenderTarget_->InitCustomFormat(RENDER_TARGET_DEFAULT_RESOLUTION, RENDER_TARGET_DEFAULT_RESOLUTION, PF_A32B32G32R32F, false);
    frontRenderTarget_->RenderTargetFormat = RTF_RGBA32f;

    frontCamera_->TextureTarget = frontRenderTarget_;
    frontCamera_->bCaptureEveryFrame = false;
    frontCamera_->bCaptureOnMovement = false;
    //frontCamera_->ShowFlags.DynamicShadows = false;

    rearRenderTarget_ = NewObject<UTextureRenderTarget2D>();
    rearRenderTarget_->InitCustomFormat(RENDER_TARGET_DEFAULT_RESOLUTION, RENDER_TARGET_DEFAULT_RESOLUTION, PF_A32B32G32R32F, false);
    rearRenderTarget_->RenderTargetFormat = RTF_RGBA32f;

    rearCamera_->TextureTarget = rearRenderTarget_;
    rearCamera_->bCaptureEveryFrame = false;
    rearCamera_->bCaptureOnMovement = false;
    //rearCamera_->ShowFlags.DynamicShadows = false;
}

void AViewSynthesizer::EndPlay(const EEndPlayReason::Type endPlayReason)
{
    Super::EndPlay(endPlayReason);

	DeleteStreams();
}

void AViewSynthesizer::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

    if (wantsStop_)
    {
        StopTransmitInternal();
    }

    const std::lock_guard<std::mutex> lock(streamMutex_);

    if (!transmitEnabled_)
    {
        return;
    }

    Capture();
}

void AViewSynthesizer::StartTransmit()
{
    const std::lock_guard<std::mutex> lock(streamMutex_);

    if (ResetStreams())
    {
        transmitEnabled_ = true;
        useEditorTick_ = true;
    }
}

void AViewSynthesizer::StopTransmit()
{
    // Stop the transmit in a synchronized manner to avoid race conditions
    wantsStop_ = true;
}

bool AViewSynthesizer::StartStreams()
{
    // TODO: More sanity checks should be added here
    if (saveDirectory_.IsEmpty() || saveDirectory_.Contains("\\") || saveDirectory_[saveDirectory_.Len() - 1] != '/')
    {
        Debug::Log("Invalid directory");

        return false;
    }

    if (!resultRenderTarget_)
    {
        Debug::Log("Result render target not set");

        return false;
    }

    // Capturing below 16x16 causes corrupted video, might be because of SSE instructions in YUV conversion
    uint16_t frameWidth = std::max(remoteStreamWidth_, 16);
    uint16_t frameHeight = std::max(remoteStreamHeight_, 16);

    // Width and height must be divisible by eight (HEVC limitation)
    // This rounds up to the nearest integers divisible by eight
    frameWidth += (8 - (frameWidth % 8)) % 8;
    frameHeight += (8 - (frameHeight % 8)) % 8;

    wantsStop_ = false;
    frameNumber_ = 0;
    startTimestampMs_ = 0;

    frontCamera_->FOVAngle = frontFov_;
    rearCamera_->FOVAngle = rearFov_;

    frontRenderTarget_->ResizeTarget(frameWidth, frameHeight);
    rearRenderTarget_->ResizeTarget(frameWidth, frameHeight);
    resultRenderTarget_->ResizeTarget(frameWidth, frameHeight);

    try
    {
        if (saveToFile_)
        {
            frontReader_ = nullptr;
            rearReader_ = new RenderTargetReaderWithUserData({ rearRenderTarget_ }, true, depthRange_);

            runners_.push_back(
                new AsyncPipelineRunner(
                    new Pipeline(
                        rearReader_,
                        new ParallelSink<2>(
                            {
                                new SequentialSink(
                                    new DepthSeparator(),
                                    new PngRecorder(TCHAR_TO_UTF8(*saveDirectory_), frameWidth, frameHeight * 2)),
                                new SequentialSink(
                                    new CsvLogger(),
                                    new FileSink(TCHAR_TO_UTF8(*(saveDirectory_ + FString("log.csv")))))
                            }))));
        }
        else
        {
            frontReader_ = new RenderTargetReaderWithUserData({ frontRenderTarget_ }, true, depthRange_);
            rearReader_ = new RenderTargetReaderWithUserData({ rearRenderTarget_ }, true, depthRange_);

            runners_.push_back(
                new AsyncPipelineRunner(
                    new Pipeline(
                        frontReader_,
                        new SequentialFilter(
                            new ParallelFilter(
                                new SequentialFilter(
                                    new DuplicatorFilter<2>(),
                                    new ParallelFilter(
                                        new RgbaToYuvConverter(frameWidth, frameHeight),
                                        new DepthToYuvConverter()),
                                    new ImageConcatenator<2>(frameWidth, frameHeight),
                                    new HevcEncoder(frameWidth, frameHeight * 2, 28,
                                        quantizationParameter_, wavefrontParallelProcessing_, overlappedWavefront_, HevcPresetMinimumLatency)),
                                new PassthroughFilter<1>()),
                            new SeiEmbedder("CiThruSViewSynth")),
                        new RtpTransmitter(TCHAR_TO_UTF8(*remoteStreamIp_), remoteStreamPort_ + 1))));

            runners_.push_back(
                new AsyncPipelineRunner(
                    new Pipeline(
                        rearReader_,
                        new SequentialFilter(
                            new ParallelFilter(
                                new SequentialFilter(
                                    new RgbaToYuvConverter(frameWidth, frameHeight),
                                    new SidechainSource(
                                        new SolidColorImageGenerator(frameWidth, frameHeight, 0, 128, 128),
                                        new ImageConcatenator<2>(frameWidth, frameHeight)),
                                    new HevcEncoder(frameWidth, frameHeight * 2, 28,
                                        quantizationParameter_, wavefrontParallelProcessing_, overlappedWavefront_, HevcPresetMinimumLatency)),
                                new PassthroughFilter<1>()),
                            new SeiEmbedder("CiThruSViewSynth")),
                        new RtpTransmitter(TCHAR_TO_UTF8(*remoteStreamIp_), remoteStreamPort_))));

            runners_.push_back(
                new AsyncPipelineRunner(
                    new Pipeline(
                        new RtpReceiver(TCHAR_TO_UTF8(*remoteStreamIp_), remoteStreamPort_ - 1),
                        new HevcDecoder(28),
                        new YuvToRgbaConverter(frameWidth, frameHeight, "bgra"),
                        new RenderTargetWriter(resultRenderTarget_))));
        }
    }
    catch (const std::exception& exception)
    {
        Debug::Log("Pipeline construction failed: " + std::string(exception.what()));

        DeleteStreams();

        return false;
    }

    return true;
}

void AViewSynthesizer::DeleteStreams()
{
    for (AsyncPipelineRunner* runner : runners_)
    {
        delete runner;
    }

    runners_.clear();

    // These are already deleted by the pipeline so don't delete them twice
    frontReader_ = nullptr;
    rearReader_ = nullptr;
}

bool AViewSynthesizer::ResetStreams()
{
    DeleteStreams();

    return StartStreams();
}

void AViewSynthesizer::StopTransmitInternal()
{
    const std::lock_guard<std::mutex> lock(streamMutex_);

    DeleteStreams();

    transmitEnabled_ = false;
    useEditorTick_ = false;
    wantsStop_ = false;
}

void AViewSynthesizer::Capture()
{
    if (frontReader_)
    {
        frontCamera_->CaptureScene();
    }

    if (rearReader_)
    {
        rearCamera_->CaptureScene();
    }

    uint64_t timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    if (startTimestampMs_ == 0)
    {
        startTimestampMs_ = timestampMs;
    }

    if (frontReader_)
    {
        FTransform frontTfm = FTransform(frontCamera_->GetComponentRotation(), frontCamera_->GetComponentLocation());

        ViewSynthCameraParams frontCameraParams;

        frontCameraParams.frameNumber = frameNumber_;

        frontCameraParams.pos = FVector3f(frontTfm.GetTranslation() / 100.0f);    // cm in Unreal, m in ViewSynth
        frontCameraParams.rot = FVector3f(frontTfm.GetRotation().Euler());

        frontCameraParams.pos = FVector3f(frontCameraParams.pos.Y, frontCameraParams.pos.Z, frontCameraParams.pos.X);
        frontCameraParams.rot = FVector3f(frontCameraParams.rot.Y, -frontCameraParams.rot.Z, frontCameraParams.rot.X);

        frontCameraParams.fov = frontCamera_->FOVAngle;
        frontCameraParams.depthRange = depthRange_;

        frontCameraParams.timestamp = timestampMs - startTimestampMs_;

        frontReader_->Read(reinterpret_cast<uint8_t*>(&frontCameraParams), sizeof(ViewSynthCameraParams));
    }

    if (rearReader_)
    {
        FTransform rearTfm = FTransform(rearCamera_->GetComponentRotation(), rearCamera_->GetComponentLocation());

        ViewSynthCameraParams rearCameraParams;

        rearCameraParams.frameNumber = frameNumber_;

        rearCameraParams.pos = FVector3f(rearTfm.GetTranslation() / 100.0f);  // cm in Unreal, m in ViewSynth
        rearCameraParams.rot = FVector3f(rearTfm.GetRotation().Euler());

        rearCameraParams.pos = FVector3f(rearCameraParams.pos.Y, rearCameraParams.pos.Z, rearCameraParams.pos.X);
        rearCameraParams.rot = FVector3f(rearCameraParams.rot.Y, -rearCameraParams.rot.Z, rearCameraParams.rot.X);

        rearCameraParams.fov = rearCamera_->FOVAngle;
        rearCameraParams.depthRange = depthRange_;

        rearCameraParams.timestamp = timestampMs - startTimestampMs_;

        rearReader_->Read(reinterpret_cast<uint8_t*>(&rearCameraParams), sizeof(ViewSynthCameraParams));
    }

    frameNumber_++;
}
