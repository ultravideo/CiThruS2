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

	virtual void SetInput(const IImageSource* source) = 0;
	virtual std::string GetInputFormat() const = 0;

protected:
	IImageSink() { }
};
