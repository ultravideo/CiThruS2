#pragma once

#include "IPipelineComponent.h"
#include "InputPin.h"

#include <cstdint>
#include <array>
#include <string>

// Base class for all pipeline components that receive input
template <uint8_t NInputs>
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
		static_assert(I < NInputs, "Input pin index out of bounds");
		return inputPins_[I];
	}

protected:
	PipelineSink() : inputPins_()
	{
		for (int i = 0; i < NInputs; i++)
		{
			inputPins_[i].SetIndex(i);
		}
	}

private:
	std::array<InputPin, NInputs> inputPins_;
};
