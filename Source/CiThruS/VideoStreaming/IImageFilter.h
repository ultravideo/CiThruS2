#pragma once

#include "IImagePipelineComponent.h"
#include "IImageSink.h"
#include "IImageSource.h"

#include <cstdint>

// Interface for image pipeline components that filter image data
class IImageFilter : public IImageSource, public IImageSink
{
public:
	virtual ~IImageFilter() { }

protected:
	IImageFilter() { }
};
