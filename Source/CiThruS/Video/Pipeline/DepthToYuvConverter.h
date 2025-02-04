#pragma once

#include "IImageFilter.h"

// Converts 8-bit depth images to YUV 4:2:0
class CITHRUS_API DepthToYuvConverter : public IImageFilter
{
public:
	DepthToYuvConverter(const float& depthRange) : outputFrame_(nullptr), outputSize_(0), depthRange_(depthRange) { }
	virtual ~DepthToYuvConverter();

	virtual void Process() override;

	virtual bool SetInput(const IImageSource* source) override;

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return "yuv420"; }

protected:
	uint8_t* const* inputFrame_;
	const uint32_t* inputSize_;

	uint8_t* outputFrame_;
	uint32_t outputSize_;

	uint16_t inputFrameWidth_;
	uint16_t inputFrameHeight_;

	uint16_t outputFrameWidth_;
	uint16_t outputFrameHeight_;

	float depthRange_;
};
