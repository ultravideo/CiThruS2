#pragma once

#include "IImageMultiSource.h"
#include "IImageSink.h"

// Interface for image pipeline components that have one input and multiple outputs
template <uint8_t N>
class IImageFork : public IImageMultiSource<N>, public IImageSink
{
public:
	virtual ~IImageFork() { };

protected:
	IImageFork() { };
};
