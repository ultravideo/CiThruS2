#pragma once

#include "Pipeline/Internal/PipelineSource.h"
#include "Pipeline/Internal/PipelineFilter.h"
#include "Pipeline/Internal/ProxySourceBase.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>
#include <tuple>

// Scaffolding for sequentially connecting filters to a source
template <uint8_t NOutputs>
class SequentialSource : public ProxySourceBase<NOutputs>
{
	// Template implementation must be in the header file

public:
	template <uint8_t NSourceOutputs, uint8_t... NFilterInputs, uint8_t... NFilterOutputs>
	SequentialSource(PipelineSource<NSourceOutputs>* source, PipelineFilter<NFilterInputs, NFilterOutputs>*... filters)
	{
		static_assert(
			std::is_same<
				std::integer_sequence<uint8_t, NFilterInputs..., NOutputs>,
				std::integer_sequence<uint8_t, NSourceOutputs, NFilterOutputs...>>::value,
			"The output count of each component must match the input count of the next component");
		static_assert(sizeof...(filters) > 0,
			"SequentialSource is meant for connecting at least one filter to a source. "
			"If you don't need any filters, you should be able to use the source by itself without any scaffolding");

		if (((filters == nullptr) || ...))
		{
			throw std::invalid_argument("Filter cannot be nullptr");
		}

		if (source == nullptr)
		{
			throw std::invalid_argument("Source cannot be nullptr");
		}

		ProxyBase::components_ = { source, filters... };

		ProxySourceBase<NInputs>::pushOutputData_ = [this, filters...]()
			{
				auto filtersTuple = std::tie(filters...);
				auto lastFilter = std::get<sizeof...(filters) - 1>(filtersTuple);

				// Push output data from the output pins of the last filter
				TemplateUtility::For<NOutputs>([&, this]<uint8_t i>()
				{
					this->template GetOutputPin<i>().SetData(lastFilter->template GetOutputPin<i>().GetData());
					this->template GetOutputPin<i>().SetSize(lastFilter->template GetOutputPin<i>().GetSize());
				});
			};

		auto filtersTuple = std::tie(filters...);
		auto firstFilter = std::get<0>(filtersTuple);
		auto lastFilter = std::get<sizeof...(filters) - 1>(filtersTuple);

		// Connect source to first filter
		TemplateUtility::For<NSourceOutputs>([&, this]<uint8_t i>()
		{
			firstFilter->template GetInputPin<i>().ConnectToOutputPin(source->template GetOutputPin<i>());
		});

		firstFilter->OnInputPinsConnected();

		// Connect filters to each other
		TemplateUtility::For<sizeof...(filters) - 1>([&]<uint8_t i>()
		{
			ConnectFilters(std::get<i>(filtersTuple), std::get<i + 1>(filtersTuple));
		});

		// Initialize output pins based on last filter
		TemplateUtility::For<NOutputs>([&, this]<uint8_t i>()
		{
			this->template GetOutputPin<i>().Initialize(this, lastFilter->template GetOutputPin<i>().GetFormat());
		});
	}

	virtual ~SequentialSource() { }

protected:
	template <uint8_t NFirstInputs, uint8_t NSharedPins, uint8_t NSecondOutputs>
	static void ConnectFilters(PipelineFilter<NFirstInputs, NSharedPins>* firstFilter, PipelineFilter<NSharedPins, NSecondOutputs>* secondFilter)
	{
		TemplateUtility::For<NSharedPins>([&]<uint8_t i>()
		{
			secondFilter->template GetInputPin<i>().ConnectToOutputPin(firstFilter->template GetOutputPin<i>());
		});

		secondFilter->OnInputPinsConnected();
	}
};

// Deduction guide so that you don't have to specify NOutputs manually when creating a new instance of this class
template <uint8_t NSourceOutputs, uint8_t... NFilterInputs, uint8_t... NFilterOutputs>
SequentialSource(PipelineSource<NSourceOutputs>*, PipelineFilter<NFilterInputs, NFilterOutputs>*...) ->
	SequentialSource<TemplateUtility::GetLastFromVector<uint8_t>({ NFilterOutputs... })>;
