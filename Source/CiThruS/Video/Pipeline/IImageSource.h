#pragma once

#include "IImagePipelineComponent.h"

#include <cstdint>
#include <string>

// Interface for image pipeline components that provide image data
class IImageSource : public virtual IImagePipelineComponent
{
public:
	virtual ~IImageSource() { }

	// The output data and size should only change in the constructor
	// and IImagePipelineComponent::Process functions to ensure there
	// are no race conditions
	virtual uint8_t* const* GetOutput() const = 0;
	virtual const uint32_t* GetOutputSize() const = 0;
	virtual std::string GetOutputFormat() const = 0;

protected:
	IImageSource() { }
};
