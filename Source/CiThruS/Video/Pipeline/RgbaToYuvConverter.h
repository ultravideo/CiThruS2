#pragma once

#if defined(__x86_64__) || defined(_M_X64) && !defined(_M_ARM64EC)
#define CITHRUS_SSE41_AVAILABLE
#else
#pragma message (__FILE__ ": warning: SSE4.1 instructions not available, unsupported CPU")
#endif // defined(...)

#include "IImageFilter.h"

// Converts RGBA or ABGR images to YUV 4:2:0
class CITHRUS_API RgbaToYuvConverter : public IImageFilter
{
public:
	RgbaToYuvConverter(const uint16_t& frameWidth, const uint16_t& frameHeight);
	virtual ~RgbaToYuvConverter();

	virtual void Process() override;

	virtual bool SetInput(const IImageSource* source) override;

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return "yuv420"; }

protected:
	uint8_t* const* inputFrame_;
	const uint32_t* inputSize_;
	std::string inputFormat_;

	uint8_t* outputFrame_;
	uint32_t outputSize_;

	uint16_t inputFrameWidth_;
	uint16_t inputFrameHeight_;

	uint16_t outputFrameWidth_;
	uint16_t outputFrameHeight_;

	void RgbaToYuvSse41(uint8_t* input, uint8_t** output, int width, int height);
};
