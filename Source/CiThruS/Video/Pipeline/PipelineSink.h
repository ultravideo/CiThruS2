#pragma once

#include "IPipelineComponent.h"
#include "InputPin.h"

#include <cstdint>
#include <array>

// Base class for all pipeline components that receive input
template <uint8_t N>
class PipelineSink : public virtual IPipelineComponent
{
public:
	virtual ~PipelineSink() { }

	virtual void OnInputPinsConnected() { };

	template <uint8_t I>
	inline InputPin& GetInputPin()
	{
		// Obtaining the pin reference through this function allows
		// compile-time bounds checking
		static_assert(I < N, "Input pin index out of bounds");
		return inputPins_[I];
	}

protected:
	PipelineSink() { }

private:
	std::array<InputPin, N> inputPins_;
};
