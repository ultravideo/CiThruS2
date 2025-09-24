#pragma once

#include "PipelineFilter.h"

// Converts RGBA data from 32-bit floats to 8-bit unsigned integers
class CITHRUS_API FloatToByteConverter : public PipelineFilter<1, 1>
{
public:
	FloatToByteConverter();
	virtual ~FloatToByteConverter();

	virtual void Process() override;
	virtual void OnInputPinsConnected() override;

protected:
	uint8_t* outputData_;
	uint32_t outputSize_;
};
