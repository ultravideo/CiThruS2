#include "RenderTargetWriter.h"
#include "PipelineSource.h"
#include "Misc/Debug.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"

RenderTargetWriter::RenderTargetWriter(UTextureRenderTarget2D* texture)
	: frameDirty_(false), initialized_(false), destroyed_(false)
{
	if (!texture)
	{
		Debug::Log("No texture provided to RenderTargetWriter");
		return;
	}

	ETextureRenderTargetFormat format = texture->RenderTargetFormat;

	switch (format)
	{
	case ETextureRenderTargetFormat::RTF_R32f:
		GetInputPin<0>().SetAcceptedFormat("gray32f");
		bytesPerPixel_ = 4 * 1;
		break;
	case ETextureRenderTargetFormat::RTF_RGBA32f:
		GetInputPin<0>().SetAcceptedFormat("rgba32f");
		bytesPerPixel_ = 4 * 4;
		break;
	case ETextureRenderTargetFormat::RTF_RGBA8:
	case ETextureRenderTargetFormat::RTF_RGBA8_SRGB:
		// For some reason UE actually expects BGRA, not RGBA...
		GetInputPin<0>().SetAcceptedFormat("bgra");
		bytesPerPixel_ = 1 * 4;
		break;
	default:
		return;
	}

	// Note that this is executed on the render thread later and not yet
	ENQUEUE_RENDER_COMMAND(InitializeReader)(
		[this, texture](FRHICommandListImmediate& RHICmdList)
		{
			resourceMutex_.lock();

			if (destroyed_)
			{
				resourceMutex_.unlock();
				return;
			}

			texture_ = texture->GetResource()->GetTexture2DRHI();
			frameWidth_ = texture_->GetDesc().Extent.X;
			frameHeight_ = texture_->GetDesc().Extent.Y;
			inputBuffer_ = new uint8_t[frameWidth_ * frameHeight_ * bytesPerPixel_];
			initialized_ = true;

			resourceMutex_.unlock();
		});
}

RenderTargetWriter::~RenderTargetWriter()
{
	resourceMutex_.lock();

	destroyed_ = true;
	initialized_ = false;

	delete[] inputBuffer_;
	inputBuffer_ = nullptr;

	texture_ = nullptr;

	resourceMutex_.unlock();
}

void RenderTargetWriter::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData || inputSize != frameWidth_ * frameHeight_ * bytesPerPixel_ || !initialized_)
	{
		return;
	}

	writeMutex_.lock();

	// We should only pass new data to the render thread if the render thread has processed the previous data!
	// Otherwise the render commands may pile up faster than UE can execute them and the program hangs
	if (frameDirty_)
	{
		writeMutex_.unlock();
		return;
	}

	// Input data must be grabbed now, there's no guarantee inputFrame_ will be valid after exiting this function
	memcpy(inputBuffer_, inputData, inputSize);

	frameDirty_ = true;

	writeMutex_.unlock();

	// Note that this is executed on the render thread later and not yet
	ENQUEUE_RENDER_COMMAND(ExtractRenderTargets)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			resourceMutex_.lock();

			if (!initialized_)
			{
				resourceMutex_.unlock();

				return;
			}

			// Write the contents of the texture
			FUpdateTextureRegion2D updateRegion = {};

			updateRegion.SrcX = 0;
			updateRegion.SrcY = 0;
			updateRegion.DestX = 0;
			updateRegion.DestY = 0;
			updateRegion.Width = frameWidth_;
			updateRegion.Height = frameHeight_;

			writeMutex_.lock();

			RHICmdList.UpdateTexture2D(texture_, 0, updateRegion, frameWidth_ * bytesPerPixel_, inputBuffer_);

			frameDirty_ = false;

			writeMutex_.unlock();

			resourceMutex_.unlock();
		});
}
