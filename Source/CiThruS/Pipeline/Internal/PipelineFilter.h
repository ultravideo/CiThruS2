#pragma once

#include "PipelineSink.h"
#include "PipelineSource.h"

// Base class for all pipeline components that both receive input and generate output
template <uint8_t NInputs, uint8_t NOutputs>
class PipelineFilter : public virtual PipelineSink<NInputs>, public virtual PipelineSource<NOutputs>
{
public:
	virtual ~PipelineFilter() { }

protected:
	PipelineFilter() { }
};
