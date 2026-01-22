#pragma once

#include "Pipeline/Internal/PipelineFilter.h"

// Converts 8-bit depth images to YUV 4:2:0
class CITHRUS_API DepthToYuvConverter : public PipelineFilter<1, 1>
{
public:
	DepthToYuvConverter();
	virtual ~DepthToYuvConverter();

	virtual void Process() override;

protected:
	uint8_t* outputData_;
	uint32_t outputSize_;
};
