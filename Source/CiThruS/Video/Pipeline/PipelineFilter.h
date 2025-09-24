#pragma once

#include "PipelineSink.h"
#include "PipelineSource.h"

// Base class for all filters
template <uint8_t N, uint8_t M>
class PipelineFilter : public PipelineSink<N>, public PipelineSource<M>
{
public:
	virtual ~PipelineFilter() { }

protected:
	PipelineFilter() { }
};
