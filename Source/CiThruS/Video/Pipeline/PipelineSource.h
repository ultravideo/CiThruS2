#pragma once

#include "IPipelineComponent.h"
#include "OutputPin.h"

#include <cstdint>
#include <array>

// Base class for all pipeline components that generate output
template <uint8_t N>
class PipelineSource : public virtual IPipelineComponent
{
public:
	virtual ~PipelineSource() { }

	template <uint8_t I>
	inline OutputPin& GetOutputPin()
	{
		// Obtaining the pin reference through this function allows
		// compile-time bounds checking
		static_assert(I < N, "Output pin index out of bounds");
		return outputPins_[I];
	}

protected:
	PipelineSource() { }

private:
	std::array<OutputPin, N> outputPins_;
};
