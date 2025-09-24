#pragma once

#if (defined(__x86_64__) || defined(_M_X64) && !defined(_M_ARM64EC))
#ifndef CITHRUS_SSE41_AVAILABLE
#define CITHRUS_SSE41_AVAILABLE
#endif // CITHRUS_SSE41_AVAILABLE
#else
#pragma message (__FILE__ ": warning: SSE4.1 instructions not available, unsupported CPU")
#endif // defined(...)

#include "PipelineFilter.h"

// Converts YUV 4:2:0 images to RGBA
class CITHRUS_API YuvToRgbaConverter : public PipelineFilter<1, 1>
{
public:
	YuvToRgbaConverter(const uint16_t& frameWidth, const uint16_t& frameHeight, const std::string& format = "rgba");
	virtual ~YuvToRgbaConverter();

	virtual void Process() override;

protected:
	uint8_t* outputData_;

	uint16_t outputFrameWidth_;
	uint16_t outputFrameHeight_;

	void YuvToRgbaSse41(const uint8_t* input, uint8_t** output, int width, int height);
};
