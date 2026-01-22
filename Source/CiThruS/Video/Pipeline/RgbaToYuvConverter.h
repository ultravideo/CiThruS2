#pragma once

#include "Optional/Sse41.h"

#include "PipelineFilter.h"

// Converts RGBA or ABGR images to YUV 4:2:0
class CITHRUS_API RgbaToYuvConverter : public PipelineFilter<1, 1>
{
public:
	RgbaToYuvConverter(const uint16_t& frameWidth, const uint16_t& frameHeight);
	virtual ~RgbaToYuvConverter();

	virtual void Process() override;

protected:
	uint8_t* outputData_;
	uint32_t outputSize_;

	uint16_t outputFrameWidth_;
	uint16_t outputFrameHeight_;

	void RgbaToYuvSse41(const uint8_t* input, uint8_t** output, int width, int height);
};
