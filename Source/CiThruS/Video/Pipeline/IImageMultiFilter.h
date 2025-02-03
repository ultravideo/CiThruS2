#pragma once

#include "IImageMultiSink.h"
#include "IImageMultiSource.h"

// Interface for image pipeline components that have multiple inputs and multiple outputs
template <uint8_t N, uint8_t M>
class IImageMultiFilter : public IImageMultiSink<N>, public IImageMultiSource<M>
{
public:
	virtual ~IImageMultiFilter() { }

protected:
	IImageMultiFilter() { }
};
