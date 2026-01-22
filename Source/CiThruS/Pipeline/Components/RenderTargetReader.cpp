#include "RenderTargetReader.h"
#include "Misc/Debug.h"

RenderTargetReader::RenderTargetReader(std::vector<UTextureRenderTarget2D*> textures, const bool& depth, const float& depthRange)
	: RenderTargetReaderBase(textures, depth, depthRange)
{
	frameBuffers_[0] = nullptr;
	frameBuffers_[1] = nullptr;

	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);
	GetOutputPin<0>().Initialize(this, imageFormat_);
}

RenderTargetReader::~RenderTargetReader()
{
	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);
}

void RenderTargetReader::Process()
{
	{
		std::unique_lock<std::mutex> lock(readMutex_);

		// Should not swap buffers if there is no new data as that would lead to showing old frames again
		if (!frameDirty_)
		{
			GetOutputPin<0>().SetData(nullptr);
			GetOutputPin<0>().SetSize(0);

			return;
		}

		// Alternate between two buffers to make sure one buffer can be read safely while the other is being written
		GetOutputPin<0>().SetData(frameBuffers_[bufferIndex_]);
		GetOutputPin<0>().SetSize(imageCount_ * frameWidth_ * frameHeight_ * bytesPerPixel_);

		bufferIndex_ = (bufferIndex_ + 1) % 2;

		frameDirty_ = false;
	}

	FlushCompleted();
}

void RenderTargetReader::Read()
{
	RenderTargetReaderBase::ReadInternal();
}
