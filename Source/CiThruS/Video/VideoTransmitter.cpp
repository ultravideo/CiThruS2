#include "VideoTransmitter.h"

#include "Pipeline/Pipeline.h"
#include "Pipeline/Components/HevcEncoder.h"
#include "Pipeline/Components/NvencHevcEncoder.h"
#include "Pipeline/Components/RtpTransmitter.h"
#include "Pipeline/Components/RenderTargetReader.h"
#include "Pipeline/Components/RenderTargetReaderWithUserData.h"
#include "Pipeline/Components/RgbaToYuvConverter.h"
#include "Pipeline/Components/Equirectangular360Converter.h"
#include "Pipeline/Components/SolidColorImageGenerator.h"
#include "Pipeline/Components/PngRecorder.h"
#include "Pipeline/Components/BgraToRgbaConverter.h"
#include "Pipeline/Components/FileSink.h"
#include "Pipeline/Components/SegmentationAnalyzer.h"
#include "Pipeline/Components/JsonLogger.h"
#include "Pipeline/Scaffolding/SequentialFilter.h"
#include "Pipeline/Scaffolding/SequentialSink.h"
#include "Pipeline/Scaffolding/ParallelSink.h"
#include "Pipeline/Scaffolding/ParallelFilter.h"
#include "Pipeline/Scaffolding/PassthroughFilter.h"
#include "Pipeline/AsyncPipelineRunner.h"
#include "ViewSynthesis/PubSubCommunicator.h"
#include "SegmentationController.h"

#include "Misc/Debug.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"

#include <string>
#include <algorithm>

AVideoTransmitter::AVideoTransmitter()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	static const FRotator CAMERA_ROTATIONS[] =
	{
		FRotator(90.0f, 0.0f, 0.0f),
		FRotator(0.0f, -90.0f, 0.0f),
		FRotator(0.0f, 0.0f, 0.0f),
		FRotator(0.0f, 90.0f, 0.0f),
		FRotator(0.0f, 180.0f, 0.0f),
		FRotator(-90.0f, 0.0f, 0.0f)
	};

	for (int i = 0; i < 6; i++)
	{
		USceneCaptureComponent2D* sceneCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(FName("360SceneCaptureComponent" + FString::FromInt(i)));
		sceneCaptureComponent->SetupAttachment(RootComponent);
		sceneCaptureComponent->SetRelativeRotation(CAMERA_ROTATIONS[i]);

		cubemapCameras_.Add(sceneCaptureComponent);
	}

	normalCamera_ = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PerspectiveSceneCaptureComponent"));
	normalCamera_->SetupAttachment(RootComponent);

	PrimaryActorTick.bCanEverTick = true;
}

void AVideoTransmitter::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	static const uint16_t RENDER_TARGET_DEFAULT_RESOLUTION = 512;

	for (USceneCaptureComponent2D* camera : cubemapCameras_)
	{
		camera->TextureTarget = NewObject<UTextureRenderTarget2D>();
		camera->TextureTarget->InitCustomFormat(RENDER_TARGET_DEFAULT_RESOLUTION, RENDER_TARGET_DEFAULT_RESOLUTION, PF_B8G8R8A8, false);
		camera->TextureTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
		camera->bCaptureEveryFrame = false;
		camera->bCaptureOnMovement = false;
		camera->CaptureSource = SCS_FinalColorHDR;
		camera->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
		camera->bAlwaysPersistRenderingState = true;
		camera->PostProcessBlendWeight = 1.0f;
		camera->ShowFlags.SetLensFlares(false);
	}

	normalCamera_->TextureTarget = NewObject<UTextureRenderTarget2D>();
	normalCamera_->TextureTarget->InitCustomFormat(RENDER_TARGET_DEFAULT_RESOLUTION, RENDER_TARGET_DEFAULT_RESOLUTION, PF_B8G8R8A8, false);
	normalCamera_->TextureTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
	normalCamera_->bCaptureEveryFrame = false;
	normalCamera_->bCaptureOnMovement = false;
	normalCamera_->CaptureSource = SCS_FinalColorHDR;
	normalCamera_->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	normalCamera_->bAlwaysPersistRenderingState = true;
	normalCamera_->PostProcessBlendWeight = 1.0f;
	normalCamera_->ShowFlags.SetLensFlares(false);
}

void AVideoTransmitter::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	StopTransmitInternal();
	Super::EndPlay(endPlayReason);
}

void AVideoTransmitter::Tick(float deltaTime)
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

	if (streamFrameRate_ > 0)
	{
		const double CaptureInterval = 1.0 / streamFrameRate_;
		captureAccumulator_ += deltaTime;
		if (captureAccumulator_ < CaptureInterval)
		{
			return;
		}

		captureAccumulator_ = FMath::Min(captureAccumulator_ - CaptureInterval, CaptureInterval);
	}

	if (segmentationReader_ != nullptr)
	{
		// Segmentation currently only supported for non-360 capture
		std::vector<SegmentedObject>* segmentationMap = USegmentationController::OptimizeIndicesForCamera(normalCamera_, frameWidth_, frameHeight_);
		normalCamera_->CaptureScene();

		segmentationReader_->Read(reinterpret_cast<uint8_t*>(segmentationMap), sizeof(segmentationMap));
	}
	else
	{
		if (capture360_)
		{
			for (USceneCaptureComponent2D* camera : cubemapCameras_)
			{
				camera->CaptureScene();
			}
		}
		else
		{
			//USegmentationController::OptimizeIndicesForCamera(normalCamera_, aspectRatio_);
			normalCamera_->CaptureScene();
		}

		reader_->Read();
	}
}

void AVideoTransmitter::StartTransmit()
{
	std::lock_guard<std::mutex> lock(streamMutex_);

	if (ResetStreams())
	{
		transmitEnabled_ = true;
		useEditorTick_ = true;
		wantsStop_ = false;
		captureAccumulator_ = 0.0;
	}
}

void AVideoTransmitter::StopTransmit()
{
	// Stop the transmit in a synchronized manner to avoid race conditions
	wantsStop_ = true;
}

bool AVideoTransmitter::StartStreams()
{
	// TODO: More sanity checks should be added here
	if (saveDirectory_.IsEmpty() || saveDirectory_.Contains("\\") || saveDirectory_[saveDirectory_.Len() - 1] != '/')
	{
		Debug::Log("Invalid directory");

		return false;
	}

	// TODO: BackBufferReader could be used, but it's dependent on the main viewport resolution and availability so it shouldn't be the default
	/*if (GEngine == nullptr || GEngine->GameViewport == nullptr || GEngine->GameViewport->Viewport == nullptr)
	{
		throw std::runtime_error("No game viewport available — video capture from the backbuffer requires PIE or a standalone game window");
	}

	const FIntPoint viewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
	frameWidth_ = viewportSize.X;
	frameHeight_ = viewportSize.Y;*/

	// Capturing below 16x16 causes corrupted video, might be because of SSE instructions in YUV conversion
	frameWidth_ = std::max(remoteStreamWidth_, 16);
	frameHeight_ = std::max(remoteStreamHeight_, 16);

	// Width and height must be divisible by eight (HEVC limitation)
	// This rounds down to the nearest integers divisible by eight
	frameWidth_ -= (frameWidth_ % 8);
	frameHeight_ -= (frameHeight_ % 8);

	capture360_ = enable360Capture_;

	SetPostProcessingMaterial(postProcessMaterial_);

	try
	{
		if (enable360Capture_)
		{
			for (USceneCaptureComponent2D* camera : cubemapCameras_)
			{
				camera->TextureTarget->ResizeTarget(widthAndHeightPerCaptureSide_, widthAndHeightPerCaptureSide_);
			}

			std::vector<UTextureRenderTarget2D*> renderTargets;
			renderTargets.resize(6, nullptr);

			for (int i = 0; i < 6; i++)
			{
				renderTargets[i] = cubemapCameras_[i]->TextureTarget;
			}

			reader_ = new RenderTargetReader(renderTargets);

			if (saveToFile_)
			{
				runner_ = new AsyncPipelineRunner(
					new Pipeline(
						reader_,
						new Equirectangular360Converter(widthAndHeightPerCaptureSide_, widthAndHeightPerCaptureSide_,
							frameWidth_, frameHeight_, bilinearFiltering_),
						new BgraToRgbaConverter(),
						new PngRecorder(TCHAR_TO_UTF8(*saveDirectory_), frameWidth_, frameHeight_)));
			}
			else
			{
				PipelineFilter<1, 1>* encoder = nullptr;

				if (useNvenc_)
				{
#ifdef CITHRUS_NVENC_AVAILABLE
					encoder = new NvencHevcEncoder(frameWidth_, frameHeight_,
						static_cast<uint8_t>(quantizationParameter_),
						HevcPresetMinimumLatency, streamFrameRate_);
#else
					Debug::Log("NVENC requested but not available. Falling back to software encoder");
#endif
				}

				if (encoder == nullptr)
				{
					encoder = new SequentialFilter<1, 1>(
						new RgbaToYuvConverter(frameWidth_, frameHeight_),
						new HevcEncoder(frameWidth_, frameHeight_,
							processingThreadCount_, quantizationParameter_, wavefrontParallelProcessing_, overlappedWavefront_,
							HevcPresetMinimumLatency, streamFrameRate_));
				}

				runner_ = new AsyncPipelineRunner(
					new Pipeline(
						reader_,
						new Equirectangular360Converter(widthAndHeightPerCaptureSide_, widthAndHeightPerCaptureSide_,
							frameWidth_, frameHeight_, bilinearFiltering_),
						encoder,
						new RtpTransmitter(TCHAR_TO_UTF8(*remoteStreamIp_), remoteVideoDstPort_, streamFrameRate_)));
			}
		}
		else
		{
			normalCamera_->FOVAngle = fov_;
			normalCamera_->TextureTarget->ResizeTarget(frameWidth_, frameHeight_);

			std::vector<UTextureRenderTarget2D*> renderTargets = { normalCamera_->TextureTarget };

			if (saveToFile_)
			{
				if (enableSegmentation_)
				{
					if (segmentationPostProcessMaterial_ == nullptr)
					{
						throw std::runtime_error("Segmentation post processing material must be set for image segmentation to work!");
					}

					SetPostProcessingMaterial(segmentationPostProcessMaterial_);

					segmentationReader_ = new RenderTargetReaderWithUserData(renderTargets, "segmentationMap");

					runner_ = new AsyncPipelineRunner(
						new Pipeline(
							segmentationReader_,
							new ParallelFilter(
								new BgraToRgbaConverter(),
								new PassthroughFilter<1>()
							),
							new SegmentationAnalyzer(frameWidth_, frameHeight_),
							new ParallelSink(
								new PngRecorder(TCHAR_TO_UTF8(*saveDirectory_), frameWidth_, frameHeight_ * 2),
								new SequentialSink(
									new JsonLogger(),
									new FileSink(TCHAR_TO_UTF8(*(saveDirectory_ + "segmentation.json")))
								)
							)
						));
				}
				else
				{
					reader_ = new RenderTargetReader(renderTargets);

					runner_ = new AsyncPipelineRunner(
						new Pipeline(
							reader_,
							new BgraToRgbaConverter(),
							new PngRecorder(TCHAR_TO_UTF8(*saveDirectory_), frameWidth_, frameHeight_)));
				}
			}
			else
			{
				reader_ = new RenderTargetReader(renderTargets);

				PipelineFilter<1, 1>* encoder = nullptr;

				if (useNvenc_)
				{
#ifdef CITHRUS_NVENC_AVAILABLE
					encoder = new NvencHevcEncoder(frameWidth_, frameHeight_,
						static_cast<uint8_t>(quantizationParameter_),
						HevcPresetMinimumLatency, streamFrameRate_);
#else
					Debug::Log("NVENC requested but not available. Falling back to software encoder");
#endif
				}

				if (encoder == nullptr)
				{
					encoder = new SequentialFilter<1, 1>(
						new RgbaToYuvConverter(frameWidth_, frameHeight_),
						new HevcEncoder(frameWidth_, frameHeight_,
							processingThreadCount_, quantizationParameter_, wavefrontParallelProcessing_, overlappedWavefront_,
							HevcPresetMinimumLatency, streamFrameRate_));
				}

				runner_ = new AsyncPipelineRunner(
					new Pipeline(
						reader_,
						encoder,
						new RtpTransmitter(TCHAR_TO_UTF8(*remoteStreamIp_), remoteVideoDstPort_, streamFrameRate_)));
			}
		}
	}
	catch (const std::exception& exception)
	{
		Debug::Log("Pipeline construction failed: " + std::string(exception.what()));

		DeleteStreams();

		return false;
	}

	UPubSubCommunicator::SetTrackedCameraForLineOfSightChecks(normalCamera_, static_cast<float>(frameWidth_) / frameHeight_);

	return true;
}

bool AVideoTransmitter::ResetStreams()
{
	DeleteStreams();

	return StartStreams();
}

void AVideoTransmitter::DeleteStreams()
{
	UPubSubCommunicator::SetTrackedCameraForLineOfSightChecks(nullptr, 1.0f);

	delete runner_;
	runner_ = nullptr;

	// These are already deleted by the pipeline so don't delete them twice
	reader_ = nullptr;
	segmentationReader_ = nullptr;
}

void AVideoTransmitter::StopTransmitInternal()
{
	std::lock_guard<std::mutex> lock(streamMutex_);

	DeleteStreams();

	transmitEnabled_ = false;
	useEditorTick_ = false;
	wantsStop_ = false;
	captureAccumulator_ = 0.0;
}

void AVideoTransmitter::SetPostProcessingMaterial(UMaterial* material)
{
	FPostProcessSettings postProcessSettings{};

	if (material != nullptr)
	{
		UMaterialInstanceDynamic* materialInstance = UMaterialInstanceDynamic::Create(material, normalCamera_);

		postProcessSettings.AddBlendable(materialInstance, 1.0f);
	}

	//postProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;
	//postProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;

	for (USceneCaptureComponent2D* camera : cubemapCameras_)
	{
		camera->PostProcessSettings = postProcessSettings;
	}

	normalCamera_->PostProcessSettings = postProcessSettings;
}
