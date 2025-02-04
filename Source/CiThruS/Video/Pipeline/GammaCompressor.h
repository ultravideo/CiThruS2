#pragma once

#include "IImageFilter.h"

// Performs gamma compression
class CITHRUS_API GammaCompressor : public IImageFilter
{
public:
	GammaCompressor(const float& gamma) : outputFrame_(nullptr), outputSize_(0), outputFormat_("error"), gamma_(gamma) { }
	virtual ~GammaCompressor();

	virtual void Process() override;

	virtual bool SetInput(const IImageSource* source) override;

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return outputFormat_; }

protected:
	uint8_t* const* inputFrame_;
	const uint32_t* inputSize_;

	uint8_t* outputFrame_;
	uint32_t outputSize_;
	std::string outputFormat_;

	uint16_t inputFrameWidth_;
	uint16_t inputFrameHeight_;

	uint16_t outputFrameWidth_;
	uint16_t outputFrameHeight_;

	float gamma_;
};
