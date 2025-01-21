#include "VideoTransmitter.h"
#include "ImagePipeline.h"
#include "HevcEncoder.h"
#include "RtpStreamer.h"
#include "RenderTargetExtractor.h"
#include "RgbaToYuvConverter.h"
#include "Equirectangular360Converter.h"
#include "AnnotationTransmitter.h"
#include "SolidColorImageGenerator.h"
#include "Misc/Debug.h"

#include <string>
#include <algorithm>

AVideoTransmitter::AVideoTransmitter()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AVideoTransmitter::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	DeleteStreams();
}

void AVideoTransmitter::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

	if (!transmitEnabled_)
	{
		return;
	}

	CaptureAndTransmit();
}

void AVideoTransmitter::ResetStreams()
{
	DeleteStreams();
	StartStreams();
}

void AVideoTransmitter::DeleteStreams()
{
	// Stop encoding thread
	if (encodingThread_.joinable())
	{
		terminateEncodingThread_ = true;
		
		encodingThread_.join();
	}

	frameNumber_ = 0;

	delete extractor_;
	extractor_ = nullptr;

	delete annotationTransmitter_;
	annotationTransmitter_ = nullptr;
}

void AVideoTransmitter::StartStreams()
{
	// Capturing below 16x16 causes corrupted video, might be because of SSE instructions in YUV conversion
	transmitFrameWidth_ = std::max(remoteStreamWidth_, 16);
	transmitFrameHeight_ = std::max(remoteStreamHeight_, 16);

	// Width and height must be divisible by eight (HEVC limitation)
	// This rounds up to the nearest integers divisible by eight
	transmitFrameWidth_ += (8 - (transmitFrameWidth_ % 8)) % 8;
	transmitFrameHeight_ += (8 - (transmitFrameHeight_ % 8)) % 8;

	transmit360Video_ = enable360Capture_;
	transmitAsync_ = transmitAsynchronously_;
	threadCount_ = std::max(processingThreadCount_, 1);

	if (transmit360Video_)
	{
		sceneCapture_->SetRenderTargetResolution(widthAndHeightPerCaptureSide_, widthAndHeightPerCaptureSide_);

		std::vector<UTextureRenderTarget2D*> renderTargets;
		renderTargets.resize(sceneCapture_->Get360CaptureTargetCount(), nullptr);

		for (int i = 0; i < sceneCapture_->Get360CaptureTargetCount(); i++)
		{
			renderTargets[i] = sceneCapture_->Get360FrameTarget(i);
		}

		extractor_ = new RenderTargetExtractor(renderTargets, threadCount_);

		pipeline_ = new ImagePipeline(
			extractor_,
			{
				new Equirectangular360Converter(widthAndHeightPerCaptureSide_, widthAndHeightPerCaptureSide_, transmitFrameWidth_, transmitFrameHeight_, bilinearFiltering_, threadCount_),
				new RgbaToYuvConverter(transmitFrameWidth_, transmitFrameHeight_),
				new HevcEncoder(transmitFrameWidth_, transmitFrameHeight_, threadCount_, quantizationParameter_, wavefrontParallelProcessing_, overlappedWavefront_)
			},
			new RtpStreamer(TCHAR_TO_UTF8(*remoteStreamIp_), remoteVideoDstPort_));
	}
	else
	{
		sceneCapture_->SetRenderTargetResolution(transmitFrameWidth_, transmitFrameHeight_);

		std::vector<UTextureRenderTarget2D*> renderTargets = { sceneCapture_->GetPerspectiveFrameTarget() };

		extractor_ = new RenderTargetExtractor(renderTargets, threadCount_);

		pipeline_ = new ImagePipeline(
			extractor_,
			{
				new RgbaToYuvConverter(transmitFrameWidth_, transmitFrameHeight_),
				new HevcEncoder(transmitFrameWidth_, transmitFrameHeight_, threadCount_, quantizationParameter_, wavefrontParallelProcessing_, overlappedWavefront_)
			},
			new RtpStreamer(TCHAR_TO_UTF8(*remoteStreamIp_), remoteVideoDstPort_));
	}

	annotationTransmitter_ = new AnnotationTransmitter(TCHAR_TO_UTF8(*remoteStreamIp_), remoteAnnotationsDstPort_);
}

void AVideoTransmitter::StartTransmit()
{
	if (!sceneCapture_)
	{
		Debug::Log("Scene capture is null, aborting");
		return;
	}

	std::lock_guard<std::mutex> lock(streamCreationMutex_);

	ResetStreams();

	sceneCapture_->EnableCameras(transmit360Video_);

	transmitEnabled_ = true;
	useEditorTick_ = true;
	terminateEncodingThread_ = false;

	if (transmitAsync_)
	{
		encodingThread_ = std::thread(&AVideoTransmitter::TransmitAsync, this);
	}
}

void AVideoTransmitter::StopTransmit()
{
	std::lock_guard<std::mutex> lock(streamCreationMutex_);

	DeleteStreams();

	if (!sceneCapture_)
	{
		return;
	}

	sceneCapture_->DisableCameras();
	transmitEnabled_ = false;
	useEditorTick_ = false;
}

void AVideoTransmitter::CaptureAndTransmit()
{
	extractor_->Extract();
	markerData_ = sceneCapture_->GetMarkerData();

	if (!transmitAsync_)
	{
		EncodeAndTransmitFrame();
	}
}

void AVideoTransmitter::EncodeAndTransmitFrame()
{
	pipeline_->Process();

	/*uint8_t* ptr = *converterPipeline_.back()->GetOutput();

	if (drawBoundingBoxes_)
	{
		for (int x = 0; x < transmitFrameWidth_; x++)
		{
			for (int y = 0; y < transmitFrameHeight_; y++)
			{
				for (const MarkerCaptureData& marker : markerData_)
				{
					if (x >= static_cast<int>(marker.boundingBoxMin.X * transmitFrameWidth_)
						&& x <= static_cast<int>(marker.boundingBoxMax.X * transmitFrameWidth_)
						&& y >= static_cast<int>(marker.boundingBoxMin.Y * transmitFrameHeight_)
						&& y <= static_cast<int>(marker.boundingBoxMax.Y * transmitFrameHeight_)
						&& !(x >= static_cast<int>(marker.boundingBoxMin.X * transmitFrameWidth_) + 1
							&& x <= static_cast<int>(marker.boundingBoxMax.X * transmitFrameWidth_) - 1
							&& y >= static_cast<int>(marker.boundingBoxMin.Y * transmitFrameHeight_) + 1
							&& y <= static_cast<int>(marker.boundingBoxMax.Y * transmitFrameHeight_) - 1))
					{
						ptr[x + y * transmitFrameWidth_] = 255;
					}
				}
			}
		}
	}*/

	if (sendAnnotations_)
	{
		annotationTransmitter_->Transmit(frameNumber_++, markerData_);
	}
}

void AVideoTransmitter::TransmitAsync()
{
	while (!terminateEncodingThread_)
	{
		std::chrono::time_point<std::chrono::high_resolution_clock> startTime = std::chrono::high_resolution_clock::now();

		EncodeAndTransmitFrame();

		std::chrono::time_point<std::chrono::high_resolution_clock> endTime = std::chrono::high_resolution_clock::now();

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(0, 5.f, FColor::Green, "transmit fps " + FString::FromInt(static_cast<int>(1000.0 / std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count())));
		}
	}
}
