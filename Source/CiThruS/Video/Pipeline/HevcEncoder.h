#pragma once

#include "Optional/Kvazaar.h"

#include "PipelineFilter.h"
#include "CoreMinimal.h"

#include <string>
#include <chrono>

enum HevcEncoderPreset : uint8_t
{
	HevcPresetNone,
	HevcPresetMinimumLatency,
	HevcPresetLossless
};

// Encodes YUV 4:2:0 data into HEVC video using Kvazaar
class CITHRUS_API HevcEncoder : public PipelineFilter<1, 1>
{
public:
	HevcEncoder(const uint16_t& frameWidth, const uint16_t& frameHeight, const uint8_t& threadCount, const uint8_t& qp, const uint8_t& wpp, const uint8_t& owf, const HevcEncoderPreset& preset = HevcPresetNone);
	virtual ~HevcEncoder();

	virtual void Process() override;

protected:
	uint32_t frameWidth_;
	uint32_t frameHeight_;

	uint8_t* outputData_;

	std::chrono::high_resolution_clock::time_point startTime_;

#ifdef CITHRUS_KVAZAAR_AVAILABLE
	const kvz_api* kvazaarApi_ = kvz_api_get(8);
	kvz_config* kvazaarConfig_;
	kvz_encoder* kvazaarEncoder_;
	kvz_picture* kvazaarTransmitPicture_;
#endif // CITHRUS_KVAZAAR_AVAILABLE
};
