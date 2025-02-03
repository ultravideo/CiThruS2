#pragma once

#include "IImageSource.h"

#include <vector>

// Generates a solid color RGBA image
class CITHRUS_API SolidColorImageGenerator : public IImageSource
{
public:
	SolidColorImageGenerator(const uint16_t& width, const uint16_t& height, const uint8_t& red, const uint8_t& green, const uint8_t& blue, const uint8_t& alpha);
	~SolidColorImageGenerator();

	virtual void Process() override;

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return "rgba"; }

protected:
	uint8_t* outputFrame_;
	uint32_t outputSize_;
};
