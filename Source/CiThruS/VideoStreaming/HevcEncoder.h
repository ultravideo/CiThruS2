#pragma once

#if __cplusplus >= 201703L || _MSC_VER > 1911
#if __has_include("../ThirdParty/Kvazaar/Include/kvazaar.h")
#define CITHRUS_KVAZAAR_AVAILABLE
#include "../ThirdParty/Kvazaar/Include/kvazaar.h"
#else
#pragma message (__FILE__ ": warning: Kvazaar not found, HEVC encoding is unavailable")
#endif // __has_include(...)
#else
#include "../ThirdParty/Kvazaar/Include/kvazaar.h"
#endif // __cplusplus(...)

#include "IImageFilter.h"

#include "CoreMinimal.h"
#include <string>

// Encodes YUV 4:2:0 data into HEVC video using Kvazaar
class CITHRUS_API HevcEncoder : public IImageFilter
{
public:
	HevcEncoder(const uint16_t& frameWidth, const uint16_t& frameHeight, const uint8_t& threadCount, const uint8_t& qp, const uint8_t& wpp, const uint8_t& owf);
	virtual ~HevcEncoder();

	virtual void Process() override;

	inline virtual void SetInput(const IImageSource* source) override
	{
		inputFrame_ = source->GetOutput();
	}
	inline virtual std::string GetInputFormat() const override { return "yuv420"; }

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return "hevc"; }

protected:
	uint32_t frameWidth_;
	uint32_t frameHeight_;

	uint8_t* const* inputFrame_;
	uint8_t* outputFrame_;
	uint32_t outputSize_;

#ifdef CITHRUS_KVAZAAR_AVAILABLE
	const kvz_api* kvazaarApi_ = kvz_api_get(8);
	kvz_config* kvazaarConfig_;
	kvz_encoder* kvazaarEncoder_;
	kvz_picture* kvazaarTransmitPicture_;
#endif // CITHRUS_KVAZAAR_AVAILABLE
};
