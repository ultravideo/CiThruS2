#pragma once

#include "Pipeline/Internal/PipelineSource.h"

#include <vector>

// Generates a solid color image
class CITHRUS_API SolidColorImageGenerator : public PipelineSource<1>
{
public:
	SolidColorImageGenerator(const uint16_t& width, const uint16_t& height, const uint8_t& red, const uint8_t& green, const uint8_t& blue, const uint8_t& alpha, const std::string& format = "rgba");
	SolidColorImageGenerator(const uint16_t& width, const uint16_t& height, const uint8_t& y, const uint8_t& u, const uint8_t& v);
	~SolidColorImageGenerator();

	virtual void Process() override;

protected:
	uint8_t* outputData_;
};
