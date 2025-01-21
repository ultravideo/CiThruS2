#pragma once

#include "IImageFilter.h"

// Converts RGBA data from 32-bit floats to 8-bit unsigned integers
class CITHRUS_API FloatToByteConverter : public IImageFilter
{
public:
	FloatToByteConverter(const uint16_t& frameWidth, const uint16_t& frameHeight, const uint8_t& threadCount);
	virtual ~FloatToByteConverter();

	virtual void Process() override;

	virtual void SetInput(const IImageSource* source) override;
	inline virtual std::string GetInputFormat() const override { return "rgba32f"; }

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return outputFormat_; }

protected:
	uint8_t* const* inputFrame_;
	uint8_t* outputFrame_;
	uint32_t outputSize_;
	std::string outputFormat_ = "error";

	uint16_t inputFrameWidth_;
	uint16_t inputFrameHeight_;

	uint16_t outputFrameWidth_;
	uint16_t outputFrameHeight_;

	uint8_t colorChannels_;
	uint8_t threadCount_;
};
