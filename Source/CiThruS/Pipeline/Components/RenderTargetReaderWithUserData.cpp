#include "RenderTargetReaderWithUserData.h"
#include "Misc/Debug.h"

RenderTargetReaderWithUserData::RenderTargetReaderWithUserData(std::vector<UTextureRenderTarget2D*> textures, const bool& depth, const float& depthRange)
	: RenderTargetReaderBase(textures, depth, depthRange)
{
	frameBuffers_[0] = nullptr;
	frameBuffers_[1] = nullptr;

	queuedUserData_ = nullptr;
	queuedUserDataSize_ = 0;

	userData_[0] = nullptr;
	userData_[1] = nullptr;
	userDataSize_ = 0;

	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);
	GetOutputPin<0>().Initialize(this, imageFormat_);

	GetOutputPin<1>().SetData(nullptr);
	GetOutputPin<1>().SetSize(0);
	GetOutputPin<1>().Initialize(this, "binary");
}

RenderTargetReaderWithUserData::~RenderTargetReaderWithUserData()
{
	delete[] userData_[0];
	delete[] userData_[1];

	userData_[0] = nullptr;
	userData_[1] = nullptr;

	delete[] queuedUserData_;

	queuedUserData_ = nullptr;

	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);

	GetOutputPin<1>().SetData(nullptr);
	GetOutputPin<1>().SetSize(0);
}

void RenderTargetReaderWithUserData::Process()
{
	{
		std::unique_lock<std::mutex> lock(readMutex_);

		// Should not swap buffers if there is no new data as that would lead to showing old frames again
		if (!frameDirty_)
		{
			GetOutputPin<0>().SetData(nullptr);
			GetOutputPin<0>().SetSize(0);

			GetOutputPin<1>().SetData(nullptr);
			GetOutputPin<1>().SetSize(0);

			return;
		}

		// Alternate between two buffers to make sure one buffer can be read safely while the other is being written
		GetOutputPin<0>().SetData(frameBuffers_[bufferIndex_]);
		GetOutputPin<0>().SetSize(imageCount_ * frameWidth_ * frameHeight_ * bytesPerPixel_);
		GetOutputPin<1>().SetData(userData_[bufferIndex_]);
		GetOutputPin<1>().SetSize(userDataSize_);

		bufferIndex_ = (bufferIndex_ + 1) % 2;

		frameDirty_ = false;
	}

	FlushCompleted();
}

void RenderTargetReaderWithUserData::AfterExtraction()
{
	delete[] userData_[bufferIndex_];
	userData_[bufferIndex_] = queuedUserData_;
	userDataSize_ = queuedUserDataSize_;

	queuedUserData_ = nullptr;
}

void RenderTargetReaderWithUserData::Read(uint8_t* userData, const uint32_t& userDataSize)
{
	RenderTargetReaderBase::ReadInternal();

	// Copy the user data so that it can be safely deleted by the caller of this function
	queuedUserData_ = new uint8_t[userDataSize];
	queuedUserDataSize_ = userDataSize;
	memcpy(queuedUserData_, userData, userDataSize);
}
