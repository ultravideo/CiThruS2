#pragma once

#include "IImagePipelineComponent.h"

#include <cstdint>
#include <string>

class IImageSource;

// Interface for image pipeline components that receive image data
class IImageSink : public virtual IImagePipelineComponent
{
public:
	virtual ~IImageSink() { }

	// Returns true if successful
	virtual bool SetInput(const IImageSource* source) = 0;

protected:
	IImageSink() { }
};
