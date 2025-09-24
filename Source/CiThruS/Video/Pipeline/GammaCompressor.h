#pragma once

#include "PipelineFilter.h"

// Performs gamma compression
class CITHRUS_API GammaCompressor : public PipelineFilter<1, 1>
{
public:
	GammaCompressor(const float& gamma);
	virtual ~GammaCompressor();

	virtual void Process() override;
	virtual void OnInputPinsConnected() override;

protected:
	uint8_t* outputData_;
	uint32_t outputSize_;

	float gamma_;
};
