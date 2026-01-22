#pragma once

#include "Pipeline/Internal/PipelineSink.h"
#include "Pipeline/Internal/ProxySinkBase.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>
#include <tuple>

// Scaffolding for a row of parallel sinks
template <uint8_t NInputs>
class ParallelSink : public ProxySinkBase<NInputs>
{
	// Template implementation must be in the header file

public:
	template <uint8_t... NSinkInputs>
	ParallelSink(PipelineSink<NSinkInputs>*... sinks)
	{
		static_assert((NSinkInputs + ...) == NInputs,
			"The total number of input pins on the sinks must match the ParallelSink's input count");
		static_assert(sizeof...(sinks) > 1,
			"ParallelSink is meant for connecting more than one sink. "
			"If you only have one sink, you should be able to use it by itself without any scaffolding");

		if (((sinks == nullptr) || ...))
		{
			throw std::invalid_argument("Sink cannot be nullptr");
		}

		ProxyBase::components_ = { sinks... };

		// Initialize input pins
		TemplateUtility::For<NInputs>([&, this]<uint8_t i>()
		{
			this->template GetInputPin<i>().Initialize(this);
		});

		ProxySinkBase<NInputs>::onInputPinsConnected_ = [this, sinks...]()
			{
				// Recursively connect all internal pins
				ConnectPins<0>(sinks...);
			};
	}

	virtual ~ParallelSink() { }

protected:
	template <uint8_t NProcessedInputs, uint8_t NCurrentInputs, uint8_t... NRemainingInputs>
	void ConnectPins(PipelineSink<NCurrentInputs>* currentSink, PipelineSink<NRemainingInputs>*... remaining)
	{
		// Connect input pins of currentSink
		TemplateUtility::For<NCurrentInputs>([&]<uint8_t i>()
		{
			currentSink->template GetInputPin<i>().ConnectToOutputPin(this->template GetInputPin<NProcessedInputs + i>().GetConnectedPin());
		});

		currentSink->OnInputPinsConnected();

		// Recursively connect the next filter
		ConnectPins<NProcessedInputs + NCurrentInputs>(remaining...);
	}

	template <uint8_t NProcessedInputs>
	void ConnectPins()
	{
		// This empty function is needed to finish the recursion when there are 0 sinks left to process
	}
};

// Deduction guide so that you don't have to specify N manually when creating a new instance of this class
template <uint8_t... NSinkInputs>
ParallelSink(PipelineSink<NSinkInputs>*...) -> ParallelSink<(NSinkInputs + ...)>;
