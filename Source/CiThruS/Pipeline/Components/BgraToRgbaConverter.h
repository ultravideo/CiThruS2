#pragma once

#include "Pipeline/Internal/PipelineFilter.h"

// Converts BGRA color data to RGBA (or vice versa)
class CITHRUS_API BgraToRgbaConverter : public PipelineFilter<1, 1>
{
public:
	BgraToRgbaConverter();
	virtual ~BgraToRgbaConverter();

	virtual void Process() override;
	virtual void OnInputPinsConnected() override;

protected:
	uint8_t* outputData_;
	uint32_t outputSize_;
};
