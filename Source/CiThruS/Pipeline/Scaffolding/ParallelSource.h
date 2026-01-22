#pragma once

#include "Pipeline/Internal/PipelineSource.h"
#include "Pipeline/Internal/ProxySourceBase.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>
#include <tuple>

// Scaffolding for a row of parallel sources
template <uint8_t NOutputs>
class ParallelSource : public ProxySourceBase<NOutputs>
{
	// Template implementation must be in the header file

public:
	template <uint8_t... NSinkOutputs>
	ParallelSource(PipelineSource<NSinkOutputs>*... sources)
	{
		static_assert((NSinkOutputs + ...) == NOutputs,
			"The total number of output pins on the sources must match the ParallelSource's output count");
		static_assert(sizeof...(sources) > 1,
			"ParallelSource is meant for connecting more than one source. "
			"If you only have one source, you should be able to use it by itself without any scaffolding");

		if (((sources == nullptr) || ...))
		{
			throw std::invalid_argument("Source cannot be nullptr");
		}

		ProxyBase::components_ = { sources... };

		TemplateUtility::For<NOutputs>([&, this]<uint8_t i>()
		{
			this->template GetInputPin<i>().AcceptAnyFormat();
		});

		ProxySourceBase<NOutputs>::pushOutputData_ = [this, sources...]()
			{
				PushOutputs<0>(sources...);
			};

		ConnectPins<0>(sources...);
	}

	virtual ~ParallelSource() { }

protected:
	template <uint8_t NProcessedOutputs, uint8_t NCurrentOutputs, uint8_t... NRemainingOutputs>
	void ConnectPins(PipelineSource<NCurrentOutputs>* currentSource, PipelineSource<NRemainingOutputs>*... remaining)
	{
		// Set pin formats for currentSource
		TemplateUtility::For<NCurrentOutputs>([&]<uint8_t i>()
		{
			this->template GetOutputPin<NProcessedOutputs + i>().SetFormat(currentSource->template GetOutputPin<i>().GetFormat());
		});

		// Recursively process the next source
		ConnectPins<NProcessedOutputs + NCurrentOutputs>(remaining...);
	}

	template <uint8_t NProcessedOutputs>
	void ConnectPins()
	{
		// This empty function is needed to finish the recursion when there are 0 sources left to process
	}

	template <uint8_t NProcessedInputs, uint8_t NCurrentInputs, uint8_t... NRemainingInputs>
	void PushOutputs(PipelineSource<NCurrentInputs>* currentFilter, PipelineSource<NRemainingInputs>*... remaining)
	{
		// Push output data from currentFilter
		TemplateUtility::For<NCurrentInputs>([&]<uint8_t i>()
		{
			this->template GetOutputPin<NProcessedInputs + i>().SetData(currentFilter->template GetOutputPin<i>().GetData());
			this->template GetOutputPin<NProcessedInputs + i>().SetSize(currentFilter->template GetOutputPin<i>().GetSize());
		});

		// Recursively push the outputs of the next filter
		PushOutputs<NProcessedInputs + NCurrentInputs>(remaining...);
	}

	template <uint8_t NProcessedInputs>
	void PushOutputs()
	{
		// This empty function is needed to finish the recursion when there are 0 sources left to process
	}
};

// Deduction guide so that you don't have to specify NOutputs manually when creating a new instance of this class
template <uint8_t... NSinkOutputs>
ParallelSource(PipelineSource<NSinkOutputs>*...) -> ParallelSource<(NSinkOutputs + ...)>;
