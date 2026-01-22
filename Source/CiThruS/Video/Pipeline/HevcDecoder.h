#pragma once

#include "Optional/OpenHevc.h"

#include "PipelineFilter.h"

// Decodes HEVC video into YUV 4:2:0 data
class CITHRUS_API HevcDecoder : public PipelineFilter<1, 1>
{
public:
	HevcDecoder(const uint8_t& threadCount);
	virtual ~HevcDecoder();

	virtual void Process() override;

protected:
	uint8_t* outputData_;
	uint32_t outputSize_;

	uint32_t bufferSizes_[2];
	uint8_t* buffers_[2];

	uint8_t bufferIndex_;

#ifdef CITHRUS_OPENHEVC_AVAILABLE
	OpenHevc_Handle handle_;
#endif // CITHRUS_OPENHEVC_AVAILABLE
};
