#pragma once

#include "IImageFilter.h"

// Extracts grayscale depth from the alpha channel of RGBA data
class CITHRUS_API DepthSeparator : public IImageFilter
{
public:
	DepthSeparator(const uint16_t& frameWidth, const uint16_t& frameHeight);
	virtual ~DepthSeparator();

	virtual void Process() override;

	virtual bool SetInput(const IImageSource* source) override;

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return "gray32f"; }

protected:
	uint8_t* const* inputFrame_;
	const uint32_t* inputSize_;

	uint8_t* outputFrame_;
	uint32_t outputSize_;

	uint16_t inputFrameWidth_;
	uint16_t inputFrameHeight_;

	uint16_t outputFrameWidth_;
	uint16_t outputFrameHeight_;
};
