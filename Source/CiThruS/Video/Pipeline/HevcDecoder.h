#pragma once

#if __has_include("../ThirdParty/OpenHEVC/Include/openHevcWrapper.h")
#ifndef CITHRUS_OPENHEVC_AVAILABLE
#define CITHRUS_OPENHEVC_AVAILABLE
#endif // CITHRUS_OPENHEVC_AVAILABLE
#include "../ThirdParty/OpenHEVC/Include/openHevcWrapper.h"
#else
#pragma message (__FILE__ ": warning: OpenHEVC not found, HEVC decoding is unavailable")
#endif // __has_include(...)

#include "IImageFilter.h"

#include "CoreMinimal.h"
#include <string>

// Decodes HEVC video into YUV 4:2:0 data
class CITHRUS_API HevcDecoder : public IImageFilter
{
public:
	HevcDecoder();
	virtual ~HevcDecoder();

	virtual void Process() override;

	virtual bool SetInput(const IImageSource* source) override;

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return "yuv420"; }

protected:
	uint8_t* const* inputFrame_;
	const uint32_t* inputSize_;

	uint8_t* outputFrame_;
	uint32_t outputSize_;

	uint32_t bufferSizes_[2];
	uint8_t* buffers_[2];

	uint8_t bufferIndex_;

	OpenHevc_Handle handle_;
};
