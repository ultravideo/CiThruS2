#pragma once

#include "IImageFunnel.h"
#include "ProxySink.h"

#include <array>

// Concatenates YUV 4:2:0 images vertically. Each image must have the same resolution
template <uint8_t width>
class ImageConcatenator : public IImageFunnel<width>
{
public:
	ImageConcatenator(const uint16_t& frameWidth, const uint16_t& frameHeight)
		: inputFrameWidth_(frameWidth), inputFrameHeight_(frameHeight),
		outputFrameWidth_(frameWidth), outputFrameHeight_(frameHeight* width)
	{
		outputSize_ = outputFrameWidth_ * outputFrameHeight_ * 3 / 2;
		outputFrame_ = new uint8_t[outputSize_];

		for (int i = 0; i < width; i++)
		{
			sinks_[i] = new ProxySink(&inputFrames_[i], nullptr, "yuv420");
		}
	}

	virtual ~ImageConcatenator()
	{
		delete[] outputFrame_;
		outputFrame_ = nullptr;
		outputSize_ = 0;

		for (int i = 0; i < width; i++)
		{
			delete sinks_[i];
			sinks_[i] = nullptr;
		}
	}

	virtual void Process() override
	{
		const int frameSize = inputFrameWidth_ * inputFrameHeight_;

		for (int i = 0; i < inputFrames_.size(); i++)
		{
			if (*inputFrames_[i] == nullptr)
			{
				continue;
			}

			// Copy Y
			memcpy(outputFrame_ + frameSize * i, *inputFrames_[i], frameSize);
			// Copy U
			memcpy(outputFrame_ + frameSize / 4 * i + frameSize * inputFrames_.size(), *inputFrames_[i] + frameSize, frameSize / 4);
			// Copy V
			memcpy(outputFrame_ + frameSize / 4 * i + frameSize * 5 / 4 * inputFrames_.size(), *inputFrames_[i] + frameSize * 5 / 4, frameSize / 4);
		}
	}

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return "yuv420"; }

protected:
	std::array<uint8_t* const*, width> inputFrames_;
	uint8_t* outputFrame_;
	uint32_t outputSize_;

	uint16_t inputFrameWidth_;
	uint16_t inputFrameHeight_;

	uint16_t outputFrameWidth_;
	uint16_t outputFrameHeight_;

	std::array<ProxySink*, width> sinks_;

	virtual IImageSink* GetSinkAtIndexInternal(const uint8_t& index) const override
	{
		return sinks_[index];
	}
};
