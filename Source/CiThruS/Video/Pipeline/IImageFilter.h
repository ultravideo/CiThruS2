#pragma once

#include "IImageSink.h"
#include "IImageSource.h"

// Interface for image pipeline components that filter image data
class IImageFilter : public IImageSink, public IImageSource
{
public:
	virtual ~IImageFilter() { }

protected:
	IImageFilter() { }
};
