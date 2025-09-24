#pragma once

#include <cstdint>

// Interface for all pipeline components
class IPipelineComponent
{
public:
	virtual ~IPipelineComponent() { }

	virtual void Process() = 0;

protected:
	IPipelineComponent() { }
};
