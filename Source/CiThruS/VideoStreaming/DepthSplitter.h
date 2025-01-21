#pragma once

#include "IImageFilter.h"

// Extracts grayscale depth from the alpha channel of RGBA data
class CITHRUS_API DepthSplitter : public IImageFilter
{
public:
	DepthSplitter(const uint16_t& frameWidth, const uint16_t& frameHeight, const uint8_t& threadCount);
	virtual ~DepthSplitter();

	virtual void Process() override;

	inline virtual void SetInput(const IImageSource* source) override
	{
		inputFrame_ = source->GetOutput();
	}
	inline virtual std::string GetInputFormat() const override { return "rgba32f"; }

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return "gray32f"; }

protected:
	uint8_t* const* inputFrame_;
	uint8_t* outputFrame_;
	uint32_t outputSize_;

	uint16_t inputFrameWidth_;
	uint16_t inputFrameHeight_;

	uint16_t outputFrameWidth_;
	uint16_t outputFrameHeight_;

	uint8_t threadCount_;
};
