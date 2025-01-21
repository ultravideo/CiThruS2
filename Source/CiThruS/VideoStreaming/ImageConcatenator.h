#pragma once

#include <vector>

#include "IImageFilter.h"

// Concatenates YUV 4:2:0 images vertically. Each image must have the same resolution
class CITHRUS_API ImageConcatenator : public IImageSource
{
public:
	ImageConcatenator(std::vector<IImageSource*> sources, const uint16_t& frameWidth, const uint16_t& frameHeight, const uint8_t& threadCount);
	virtual ~ImageConcatenator();

	virtual void Process() override;

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return "yuv420"; }

protected:
	std::vector<uint8_t* const*> inputFrames_;
	uint8_t* outputFrame_;
	uint32_t outputSize_;

	uint16_t inputFrameWidth_;
	uint16_t inputFrameHeight_;

	uint16_t outputFrameWidth_;
	uint16_t outputFrameHeight_;

	uint8_t threadCount_;
};
