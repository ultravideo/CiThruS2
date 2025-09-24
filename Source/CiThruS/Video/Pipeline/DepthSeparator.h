#pragma once

#include "PipelineFilter.h"

// Extracts grayscale depth from the alpha channel of RGBA data
class CITHRUS_API DepthSeparator : public PipelineFilter<1, 1>
{
public:
	DepthSeparator();
	virtual ~DepthSeparator();

	virtual void Process() override;

protected:
	uint8_t* outputData_;
	uint32_t outputSize_;
};
