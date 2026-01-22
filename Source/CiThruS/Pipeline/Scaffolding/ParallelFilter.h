#pragma once

#include "Pipeline/Internal/PipelineFilter.h"
#include "Pipeline/Internal/ProxyFilterBase.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>
#include <tuple>

// Scaffolding for a row of parallel filters
template <uint8_t NInputs, uint8_t NOutputs>
class ParallelFilter : public ProxyFilterBase<NInputs, NOutputs>
{
	// Template implementation must be in the header file

public:
	template <uint8_t... NFilterInputs, uint8_t... NFilterOutputs>
	ParallelFilter(PipelineFilter<NFilterInputs, NFilterOutputs>*... filters)
	{
		static_assert((NFilterInputs + ...) == NInputs,
			"The total number of input pins on the filters must match the ParallelFilter's input count");
		static_assert((NFilterOutputs + ...) == NOutputs,
			"The total number of output pins on the filters must match the ParallelFilter's output count");
		static_assert(sizeof...(filters) > 1,
			"ParallelFilter is meant for connecting more than one filter. "
			"If you only have one filter, you should be able to use it by itself without any scaffolding");

		if (((filters == nullptr) || ...))
		{
			throw std::invalid_argument("Filter cannot be nullptr");
		}

		ProxyBase::components_ = { filters... };

		// Initialize input pins
		TemplateUtility::For<NInputs>([&, this]<uint8_t i>()
		{
			this->template GetInputPin<i>().Initialize(this);
		});

		ProxySinkBase<NInputs>::onInputPinsConnected_ = [this, filters...]()
			{
				// Recursively connect all internal pins
				ConnectPins<0, 0>(filters...);
			};

		ProxySourceBase<NOutputs>::pushOutputData_ = [this, filters...]()
			{
				// Recursively push outputs from all internal filters
				PushOutputs<0>(filters...);
			};
	}

	virtual ~ParallelFilter() { }

protected:
	template <
		uint8_t NProcessedInputs, uint8_t NProcessedOutputs,
		uint8_t NCurrentInputs, uint8_t NCurrentOutputs,
		uint8_t... NRemainingInputs, uint8_t... NRemainingOutputs>
	void ConnectPins(
		PipelineFilter<NCurrentInputs, NCurrentOutputs>* currentFilter,
		PipelineFilter<NRemainingInputs, NRemainingOutputs>*... remaining)
	{
		// Connect input pins of currentFilter
		TemplateUtility::For<NCurrentInputs>([&]<uint8_t i>()
		{
			currentFilter->template GetInputPin<i>().ConnectToOutputPin(this->template GetInputPin<NProcessedInputs + i>().GetConnectedPin());
		});

		currentFilter->OnInputPinsConnected();

		// Connect output pins of currentFilter
		TemplateUtility::For<NCurrentOutputs>([&]<uint8_t i>()
		{
			this->template GetOutputPin<NProcessedOutputs + i>().Initialize(this, currentFilter->template GetOutputPin<i>().GetFormat());
		});

		// Recursively connect the next filter
		ConnectPins<NProcessedInputs + NCurrentInputs, NProcessedOutputs + NCurrentOutputs>(remaining...);
	}

	template <uint8_t NProcessedInputs, uint8_t NProcessedOutputs>
	void ConnectPins()
	{
		// This empty function is needed to finish the recursion when there are 0 filters left to process
	}

	template <
		uint8_t NProcessedOutputs,
		uint8_t NCurrentInputs, uint8_t NCurrentOutputs,
		uint8_t... NRemainingInputs, uint8_t... NRemainingOutputs>
	void PushOutputs(
		PipelineFilter<NCurrentInputs, NCurrentOutputs>* currentFilter,
		PipelineFilter<NRemainingInputs, NRemainingOutputs>*... remaining)
	{
		// Push output data from currentFilter
		TemplateUtility::For<NCurrentOutputs>([&]<uint8_t i>()
		{
			this->template GetOutputPin<NProcessedOutputs + i>().SetData(currentFilter->template GetOutputPin<i>().GetData());
			this->template GetOutputPin<NProcessedOutputs + i>().SetSize(currentFilter->template GetOutputPin<i>().GetSize());
		});

		// Recursively push the outputs of the next filter
		PushOutputs<NProcessedOutputs + NCurrentOutputs>(remaining...);
	}

	template <uint8_t NProcessedOutputs>
	void PushOutputs()
	{
		// This empty function is needed to finish the recursion when there are 0 filters left to process
	}
};

// Deduction guide so that you don't have to manually specify NInputs and NOutputs when creating a new instance of this class
template <uint8_t... NFilterInputs, uint8_t... NFilterOutputs>
ParallelFilter(PipelineFilter<NFilterInputs, NFilterOutputs>*...) ->
	ParallelFilter<(NFilterInputs + ...), (NFilterOutputs + ...)>;
