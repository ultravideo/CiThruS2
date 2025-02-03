#pragma once

#include "IImagePipelineComponent.h"

#include <cstdint>

// Interface for all image pipeline components
class IImagePipelineComponent
{
public:
	virtual ~IImagePipelineComponent() { }

	virtual void Process() = 0;

protected:
	IImagePipelineComponent() { }
};
