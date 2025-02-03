#pragma once

#include "IImageMultiSink.h"
#include "IImageSource.h"

// Interface for image pipeline components that have multiple inputs and one output
template <uint8_t N>
class IImageFunnel : public IImageMultiSink<N>, public IImageSource
{
public:
	virtual ~IImageFunnel() { };

protected:
	IImageFunnel() { };
};
