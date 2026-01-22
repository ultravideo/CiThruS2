#pragma once

#include "IPipelineComponent.h"
#include "OutputPin.h"

#include <cstdint>
#include <array>

// Base class for all pipeline components that generate output
template <uint8_t NOutputs>
class PipelineSource : public virtual IPipelineComponent
{
public:
	virtual ~PipelineSource() { }

	template <uint8_t I>
	inline OutputPin& GetOutputPin()
	{
		// Obtaining the pin reference through this function allows
		// compile-time bounds checking
		static_assert(I < NOutputs, "Output pin index out of bounds");
		return outputPins_[I];
	}

protected:
	PipelineSource() : outputPins_()
	{
		for (int i = 0; i < NOutputs; i++)
		{
			outputPins_[i].SetIndex(i);
		}
	}

private:
	std::array<OutputPin, NOutputs> outputPins_;
};
