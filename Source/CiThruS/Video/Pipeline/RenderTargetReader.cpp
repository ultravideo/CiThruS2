#include "RenderTargetReader.h"
#include "Misc/Debug.h"
#include "Engine/TextureRenderTarget2D.h"

RenderTargetReader::RenderTargetReader(std::vector<UTextureRenderTarget2D*> textures)
	: outputFrame_(nullptr), outputSize_(0), frameDirty_(false), initialized_(false), destroyed_(false)
{
	if (!textures.empty())
	{
		ETextureRenderTargetFormat format = textures[0]->RenderTargetFormat;

		switch (format)
		{
		case ETextureRenderTargetFormat::RTF_R32f:
			outputFormat_ = "gray32f";
			bytesPerPixel_ = 4 * 1;
			break;
		case ETextureRenderTargetFormat::RTF_RGBA32f:
			outputFormat_ = "rgba32f";
			bytesPerPixel_ = 4 * 4;
			break;
		case ETextureRenderTargetFormat::RTF_RGBA8:
		case ETextureRenderTargetFormat::RTF_RGBA8_SRGB:
			// For some reason UE actually outputs BGRA, not RGBA...
			outputFormat_ = "bgra";
			bytesPerPixel_ = 1 * 4;
			break;
		default:
			outputFormat_ = "error";
			return;
		}
	}
	else
	{
		outputFormat_ = "error";
		return;
	}

	// Note that this is executed on the render thread later and not yet
	ENQUEUE_RENDER_COMMAND(InitializeReader)(
		[this, textures](FRHICommandListImmediate& RHICmdList)
		{
			resourceMutex_.lock();

			if (destroyed_)
			{
				resourceMutex_.unlock();
				return;
			}

			textures_.resize(textures.size(), nullptr);

			for (int i = 0; i < textures.size(); i++)
			{
				textures_[i] = textures[i]->GetResource()->GetTexture2DRHI();
			}

			EPixelFormat pixelFormat = EPixelFormat();

			if (!textures_.empty())
			{
				// Assumes every texture is the same size, otherwise they could be hard to concatenate
				frameWidth_ = textures_[0]->GetDesc().Extent.X;
				frameHeight_ = textures_[0]->GetDesc().Extent.Y;

				pixelFormat = textures[0]->GetFormat();
			}

			// Vulkan doesn't support copying texture data to a specific offset in a CPU-accessible texture so a separate
			// buffer is needed for concatenating the 360 cubemap sides on the GPU. In DX11/12 it is supported, so this can be skipped
			if (textures.size() != 1 && FString(GDynamicRHI->GetName()) == TEXT("Vulkan"))
			{
				FRHITextureDesc concatTextureDesc(ETextureDimension::Texture2D, ETextureCreateFlags::None, pixelFormat, FClearValueBinding::Black, FIntPoint(textures_.size() * frameWidth_, frameHeight_), 1, 1, 1, 1, 0);
				FRHITextureCreateDesc concatCreateDesc(concatTextureDesc, ERHIAccess::DSVRead | ERHIAccess::DSVWrite, TEXT("Render Target Reader Concatenation Texture"));

				concatBuffer_ = RHICmdList.CreateTexture(concatCreateDesc);
			}

			FRHITextureDesc stagingTextureDesc(ETextureDimension::Texture2D, ETextureCreateFlags::CPUReadback, pixelFormat, FClearValueBinding::Black, FIntPoint(textures_.size() * frameWidth_, frameHeight_), 1, 1, 1, 1, 0);
			FRHITextureCreateDesc stagingCreateDesc(stagingTextureDesc, ERHIAccess::CPURead | ERHIAccess::DSVWrite, TEXT("Render Target Reader Staging Texture"));

			stagingBuffer_ = RHICmdList.CreateTexture(stagingCreateDesc);

			outputSize_ = textures_.size() * frameWidth_ * frameHeight_ * bytesPerPixel_;

			frameBuffers_[0] = new uint8_t[outputSize_]();
			frameBuffers_[1] = new uint8_t[outputSize_]();

			outputFrame_ = frameBuffers_[0];
			bufferIndex_ = 1;

			initialized_ = true;

			resourceMutex_.unlock();
		});
}

RenderTargetReader::~RenderTargetReader()
{
	resourceMutex_.lock();

	destroyed_ = true;
	initialized_ = false;

	textures_.clear();

	if (concatBuffer_)
	{
		concatBuffer_ = nullptr;
	}

	if (stagingBuffer_)
	{
		stagingBuffer_ = nullptr;
	}

	delete[] frameBuffers_[0];
	delete[] frameBuffers_[1];

	frameBuffers_[0] = nullptr;
	frameBuffers_[1] = nullptr;

	resourceMutex_.unlock();
}

void RenderTargetReader::Process()
{
	readMutex_.lock();

	// Should not swap buffers if there is no new data as that would lead to showing old frames again
	if (!frameDirty_)
	{
		readMutex_.unlock();

		return;
	}

	// Alternate between two buffers to make sure one buffer can be read safely while the other is being written
	outputFrame_ = frameBuffers_[bufferIndex_];
	bufferIndex_ = (bufferIndex_ + 1) % 2;

	frameDirty_ = false;

	readMutex_.unlock();
}

void RenderTargetReader::Read()
{
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

			// Copy render targets to the staging texture, with an extra concatenation buffer or not
			if (concatBuffer_)
			{
				for (int i = 0; i < textures_.size(); i++)
				{
					FRHICopyTextureInfo copyTextureInfo = {};

					copyTextureInfo.DestPosition = FIntVector(i * frameWidth_, 0, 0);
					copyTextureInfo.Size = FIntVector(frameWidth_, frameHeight_, 1);

					RHICmdList.CopyTexture(textures_[i], concatBuffer_, copyTextureInfo);
				}

				FRHICopyTextureInfo copyTextureInfo = {};

				RHICmdList.CopyTexture(concatBuffer_, stagingBuffer_, copyTextureInfo);
			}
			else
			{
				for (int i = 0; i < textures_.size(); i++)
				{
					FRHICopyTextureInfo copyTextureInfo = {};

					copyTextureInfo.DestPosition = FIntVector(i * frameWidth_, 0, 0);
					copyTextureInfo.Size = FIntVector(frameWidth_, frameHeight_, 1);

					RHICmdList.CopyTexture(textures_[i], stagingBuffer_, copyTextureInfo);
				}
			}

			// Read the contents of the staging texture
			void* resource;
			int32_t stagingBufferWidth;
			int32_t stagingBufferHeight;
			RHICmdList.MapStagingSurface(stagingBuffer_, resource, stagingBufferWidth, stagingBufferHeight);

			const int32_t numFrames = textures_.size();

			readMutex_.lock();

			if (stagingBufferWidth == numFrames * frameWidth_ && stagingBufferHeight == frameHeight_)
			{
				// Mapped texture data has no padding: multiple rows can be copied in a single memcpy
				memcpy(
					frameBuffers_[bufferIndex_],
					reinterpret_cast<uint8_t*>(resource),
					stagingBufferHeight * stagingBufferWidth * bytesPerPixel_);
			}
			else
			{
				// Mapped texture data has padding: must be copied row by row to discard the padding
				const int32_t rgbaPitch = numFrames * frameWidth_ * bytesPerPixel_;
				const int32_t stagingPitch = stagingBufferWidth * bytesPerPixel_;

				for (int j = 0; j < stagingBufferHeight; j++)
				{
					memcpy(
						frameBuffers_[bufferIndex_] + j * rgbaPitch,
						reinterpret_cast<uint8_t*>(resource) + j * stagingPitch,
						rgbaPitch);
				}
			}

			RHICmdList.UnmapStagingSurface(stagingBuffer_);

			frameDirty_ = true;

			readMutex_.unlock();
			resourceMutex_.unlock();
		});
}
