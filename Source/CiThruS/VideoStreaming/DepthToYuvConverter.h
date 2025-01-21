#pragma once

#include "IImageFilter.h"

// Converts 8-bit depth images to YUV 4:2:0
class CITHRUS_API DepthToYuvConverter : public IImageFilter
{
public:
	DepthToYuvConverter(const uint16_t& frameWidth, const uint16_t& frameHeight, const float& depthRange, const uint8_t& threadCount);
	virtual ~DepthToYuvConverter();

	virtual void Process() override;

	inline virtual void SetInput(const IImageSource* source) override
	{
		inputFrame_ = source->GetOutput();
	}
	inline virtual std::string GetInputFormat() const override { return "gray32f"; }

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return "yuv420"; }

protected:
	uint8_t* const* inputFrame_;
	uint8_t* outputFrame_;
	uint32_t outputSize_;

	uint16_t inputFrameWidth_;
	uint16_t inputFrameHeight_;

	uint16_t outputFrameWidth_;
	uint16_t outputFrameHeight_;

	float depthRange_;
	uint8_t threadCount_;
};
