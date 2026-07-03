#include "BackBufferReader.h"
#include "Misc/CithrusGameViewportClient.h"
#include "Misc/Debug.h"

#include "RenderingThread.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "UnrealClient.h"

BackBufferReader::BackBufferReader(const uint16_t& frameWidth, const uint16_t& frameHeight)
	: frameWidth_(frameWidth),
	  frameHeight_(frameHeight),
	  bytesPerPixel_(4),
	  bufferIndex_(0),
	  frameDirty_(false),
	  stagingReady_(false),
	  sourceFormat_(PF_Unknown),
	  loggedFirstEvent_(false),
	  loggedFormatReject_(false),
	  loggedSizeReject_(false),
	  loggedFirstCapture_(false),
	  loggedNullViewport_(false),
	  loggedNullRT_(false),
	  destroyed_(false)
{
	frameBuffers_[0] = nullptr;
	frameBuffers_[1] = nullptr;

	const uint32_t bufferSize = static_cast<uint32_t>(frameWidth_) * frameHeight_ * bytesPerPixel_;
	frameBuffers_[0] = new uint8_t[bufferSize]();
	frameBuffers_[1] = new uint8_t[bufferSize]();

	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);
	GetOutputPin<0>().Initialize(this, "bgra");

	// Subscribe to the post-draw broadcast emitted by UCithrusGameViewportClient::Draw.
	// The delegate is TS multicast so Remove() is safe from the AsyncPipelineRunner
	// worker thread that destroys this reader.
	postDrawDelegateHandle_ = UCithrusGameViewportClient::OnPostDraw().AddRaw(
		this, &BackBufferReader::OnViewportPostDraw_GameThread);

	UE_LOG(LogTemp, Display,
		TEXT("BackBufferReader: constructed for %dx%d, subscribed to UCithrusGameViewportClient::OnPostDraw"),
		frameWidth_, frameHeight_);
}

BackBufferReader::~BackBufferReader()
{
	// TS multicast delegate - Remove is safe from any thread.
	if (postDrawDelegateHandle_.IsValid())
	{
		UCithrusGameViewportClient::OnPostDraw().Remove(postDrawDelegateHandle_);
	}

	postDrawDelegateHandle_.Reset();

	{
		std::lock_guard<std::mutex> lock(resourceMutex_);

		destroyed_ = true;

		if (stagingBuffer_.IsValid())
		{
			stagingBuffer_ = nullptr;
		}
	}

	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);

	delete[] frameBuffers_[0];
	delete[] frameBuffers_[1];

	frameBuffers_[0] = nullptr;
	frameBuffers_[1] = nullptr;
}

void BackBufferReader::Process()
{
	std::unique_lock<std::mutex> lock(readMutex_);

	// No new frame since the previous Process: signal "no data" so downstream filters skip
	if (!frameDirty_)
	{
		GetOutputPin<0>().SetData(nullptr);
		GetOutputPin<0>().SetSize(0);
		return;
	}

	GetOutputPin<0>().SetData(frameBuffers_[bufferIndex_]);
	GetOutputPin<0>().SetSize(static_cast<uint32_t>(frameWidth_) * frameHeight_ * bytesPerPixel_);

	// Next write goes to the other buffer so downstream can keep consuming this one
	bufferIndex_ = (bufferIndex_ + 1) % 2;

	frameDirty_ = false;
}

void BackBufferReader::OnViewportPostDraw_GameThread(FViewport* viewport)
{
	if (destroyed_)
	{
		return;
	}

	if (viewport == nullptr)
	{
		if (!loggedNullViewport_)
		{
			UE_LOG(LogTemp, Warning, TEXT("BackBufferReader: OnPostDraw fired with null FViewport"));
			loggedNullViewport_ = true;
		}
		return;
	}

	// Do NOT query viewport->GetRenderTargetTexture() from the game thread. In
	// standalone / packaged builds the renderer sets the RT pointer on the render
	// thread, so on game thread it appears null. Enqueue the capture; by the time
	// the render thread runs this lambda the scene-render command (also enqueued by
	// Super::Draw) has finished, and the FViewport's RT pointer is live. The
	// FViewport itself is owned by the engine and stable across frames, so capturing
	// it by raw pointer is safe.
	ENQUEUE_RENDER_COMMAND(BackBufferReaderCapture)(
		[this, viewport](FRHICommandListImmediate& RHICmdList)
		{
			if (destroyed_)
			{
				return;
			}

			FTextureRHIRef sourceTexture = viewport->GetRenderTargetTexture();

			if (!sourceTexture.IsValid())
			{
				if (!loggedNullRT_)
				{
					UE_LOG(LogTemp, Warning,
						TEXT("BackBufferReader: viewport->GetRenderTargetTexture() returned null on the render thread too — the viewport has no RHI texture at all in this build configuration"));
					loggedNullRT_ = true;
				}
				return;
			}

			Capture_RenderThread(RHICmdList, sourceTexture);
		});
}

void BackBufferReader::Capture_RenderThread(FRHICommandListImmediate& RHICmdList, FTextureRHIRef sourceTexture)
{
	std::lock_guard<std::mutex> resourceLock(resourceMutex_);

	if (destroyed_ || !sourceTexture.IsValid())
	{
		return;
	}

	const FIntPoint sourceSize = sourceTexture->GetDesc().Extent;
	const EPixelFormat sourceFormat = sourceTexture->GetDesc().Format;

	if (!loggedFirstEvent_)
	{
		UE_LOG(LogTemp, Display,
			TEXT("BackBufferReader: first viewport RT seen - size=%dx%d, format=%d, expected=%dx%d"),
			sourceSize.X, sourceSize.Y, static_cast<int32>(sourceFormat), frameWidth_, frameHeight_);
		loggedFirstEvent_ = true;
	}

	if (sourceSize.X < frameWidth_ || sourceSize.Y < frameHeight_)
	{
		if (!loggedSizeReject_)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("BackBufferReader: viewport RT %dx%d smaller than configured %dx%d - frames rejected. Restart StartTransmit after resizing the window"),
				sourceSize.X, sourceSize.Y, frameWidth_, frameHeight_);
			loggedSizeReject_ = true;
		}
		return;
	}

	// Supported: 8-bit BGRA/RGBA (direct copy) and 10-bit A2B10G10R10 (HDR swap-chain;
	// unpacked to 8-bit BGRA on the CPU during readback below).
	if (sourceFormat != PF_B8G8R8A8
		&& sourceFormat != PF_R8G8B8A8
		&& sourceFormat != PF_A2B10G10R10)
	{
		if (!loggedFormatReject_)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("BackBufferReader: unsupported viewport RT format %d - frames will not be captured"),
				static_cast<int32>(sourceFormat));
			loggedFormatReject_ = true;
		}
		return;
	}

	if (!stagingReady_)
	{
		sourceFormat_ = sourceFormat;

		FRHITextureCreateDesc stagingDesc = FRHITextureCreateDesc::Create2D(
			TEXT("BackBufferReaderStagingTexture"),
			FIntPoint(frameWidth_, frameHeight_),
			sourceFormat_)
			.SetFlags(ETextureCreateFlags::CPUReadback)
			.SetInitialState(ERHIAccess::CopyDest);

		stagingBuffer_ = RHICmdList.CreateTexture(stagingDesc);

		if (!stagingBuffer_.IsValid())
		{
			return;
		}

		stagingReady_ = true;
	}
	else if (sourceFormat != sourceFormat_)
	{
		// Staging texture was created for a different format; the viewport RT format changed
		// mid-stream (rare - usually only after a fullscreen/HDR toggle). Skip until restart.
		return;
	}

	RHICmdList.Transition(FRHITransitionInfo(sourceTexture, ERHIAccess::Unknown, ERHIAccess::CopySrc));
	RHICmdList.Transition(FRHITransitionInfo(stagingBuffer_, ERHIAccess::Unknown, ERHIAccess::CopyDest));

	FRHICopyTextureInfo copyInfo;
	copyInfo.Size = FIntVector(frameWidth_, frameHeight_, 1);

	RHICmdList.CopyTexture(sourceTexture, stagingBuffer_, copyInfo);

	void* mappedData = nullptr;
	int32 mappedWidth = 0;
	int32 mappedHeight = 0;

	RHICmdList.MapStagingSurface(stagingBuffer_, mappedData, mappedWidth, mappedHeight);

	if (mappedData != nullptr)
	{
		std::lock_guard<std::mutex> readLock(readMutex_);

		const int32 rowsToCopy = FMath::Min<int32>(mappedHeight, frameHeight_);
		const int32 destRowBytes = frameWidth_ * bytesPerPixel_;
		const int32 srcRowBytes = mappedWidth * bytesPerPixel_;

		if (sourceFormat_ == PF_A2B10G10R10)
		{
			// 10-10-10-2 packed -> 8-bit BGRA. Each source pixel is one uint32_t with R in the low 10 bits, then G, then B; we drop alpha and write 0xFF
			for (int32 row = 0; row < rowsToCopy; row++)
			{
				const uint32_t* srcRow = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(mappedData) + row * srcRowBytes);
				uint8_t* dstRow = frameBuffers_[bufferIndex_] + row * destRowBytes;

				for (int32 col = 0; col < frameWidth_; col++)
				{
					const uint32_t packed = srcRow[col];
					const uint32_t r10 = packed & 0x3FFu;
					const uint32_t g10 = (packed >> 10) & 0x3FFu;
					const uint32_t b10 = (packed >> 20) & 0x3FFu;

					uint8_t* px = dstRow + col * 4;
					px[0] = static_cast<uint8_t>(b10 >> 2);
					px[1] = static_cast<uint8_t>(g10 >> 2);
					px[2] = static_cast<uint8_t>(r10 >> 2);
					px[3] = 0xFF;
				}
			}
		}
		else if (mappedWidth == frameWidth_ && mappedHeight == frameHeight_)
		{
			// Unpitched 8-bit BGRA/RGBA: one bulk memcpy
			memcpy(
				frameBuffers_[bufferIndex_],
				reinterpret_cast<uint8_t*>(mappedData),
				static_cast<uint32_t>(mappedWidth) * mappedHeight * bytesPerPixel_);
		}
		else
		{
			// Pitched mapping: copy row by row to drop the GPU-added padding at the end of each row
			for (int32 row = 0; row < rowsToCopy; row++)
			{
				memcpy(
					frameBuffers_[bufferIndex_] + row * destRowBytes,
					reinterpret_cast<uint8_t*>(mappedData) + row * srcRowBytes,
					destRowBytes);
			}
		}

		frameDirty_ = true;

		if (!loggedFirstCapture_)
		{
			UE_LOG(LogTemp, Display,
				TEXT("BackBufferReader: first frame captured (%dx%d, mappedPitch=%d)"),
				frameWidth_, frameHeight_, mappedWidth);
			loggedFirstCapture_ = true;
		}
	}

	RHICmdList.UnmapStagingSurface(stagingBuffer_);
}
