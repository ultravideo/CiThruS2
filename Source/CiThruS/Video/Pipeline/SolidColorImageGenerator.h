#pragma once

#include "IImageSource.h"

#include <vector>

// Generates a solid color RGBA image
class CITHRUS_API SolidColorImageGenerator : public IImageSource
{
public:
	SolidColorImageGenerator(const uint16_t& width, const uint16_t& height, const uint8_t& red, const uint8_t& green, const uint8_t& blue, const uint8_t& alpha, const std::string& format = "rgba");
	SolidColorImageGenerator(const uint16_t& width, const uint16_t& height, const uint8_t& y, const uint8_t& u, const uint8_t& v);
	~SolidColorImageGenerator();

	virtual void Process() override;

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return outputFormat_; }

protected:
	uint8_t* outputFrame_;
	uint32_t outputSize_;
	std::string outputFormat_;
};
