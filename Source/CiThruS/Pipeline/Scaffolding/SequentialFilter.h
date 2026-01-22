#pragma once

#include "Pipeline/Internal/PipelineFilter.h"
#include "Pipeline/Internal/ProxyFilterBase.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>
#include <tuple>

// Scaffolding for sequentially connected filters
template <uint8_t NInputs, uint8_t NOutputs>
class SequentialFilter : public ProxyFilterBase<NInputs, NOutputs>
{
	// Template implementation must be in the header file

public:
	template <uint8_t... NFilterInputs, uint8_t... NFilterOutputs>
	SequentialFilter(PipelineFilter<NFilterInputs, NFilterOutputs>*... filters)
	{
		static_assert(
			std::is_same<
				std::integer_sequence<uint8_t, NFilterInputs..., NOutputs>,
				std::integer_sequence<uint8_t, NInputs, NFilterOutputs...>>::value,
			"The output count of each component must match the input count of the next component");
		static_assert(sizeof...(filters) > 1,
			"SequentialFilter is meant for connecting more than one filter. "
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
				auto filtersTuple = std::tie(filters...);
				auto firstFilter = std::get<0>(filtersTuple);
				auto lastFilter = std::get<sizeof...(filters) - 1>(filtersTuple);

				// Connect input pins to first filter
				TemplateUtility::For<NInputs>([&, this]<uint8_t i>()
				{
					firstFilter->template GetInputPin<i>().ConnectToOutputPin(this->template GetInputPin<i>().GetConnectedPin());
				});

				firstFilter->OnInputPinsConnected();

				// Connect internal filters to each other
				TemplateUtility::For<sizeof...(filters) - 1>([&]<uint8_t i>()
				{
					ConnectFilters(std::get<i>(filtersTuple), std::get<i + 1>(filtersTuple));
				});

				// Initialize output pins based on last filter
				TemplateUtility::For<NOutputs>([&, this]<uint8_t i>()
				{
					this->template GetOutputPin<i>().Initialize(this, lastFilter->template GetOutputPin<i>().GetFormat());
				});
			};

		ProxySourceBase<NOutputs>::pushOutputData_ = [this, filters...]()
			{
				auto filtersTuple = std::tie(filters...);
				auto lastFilter = std::get<sizeof...(filters) - 1>(filtersTuple);

				// Push output data from the last filter's outputs
				TemplateUtility::For<NOutputs>([&, this]<uint8_t i>()
				{
					this->template GetOutputPin<i>().SetData(lastFilter->template GetOutputPin<i>().GetData());
					this->template GetOutputPin<i>().SetSize(lastFilter->template GetOutputPin<i>().GetSize());
				});
			};
	}

	virtual ~SequentialFilter() { }

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

// Deduction guide so that you don't have to specify NInputs and NOutputs manually when creating a new instance of this class
template <uint8_t... NFilterInputs, uint8_t... NFilterOutputs>
SequentialFilter(PipelineFilter<NFilterInputs, NFilterOutputs>*...) ->
	SequentialFilter<
		TemplateUtility::GetFirstFromVector<uint8_t>({ NFilterInputs... }),
		TemplateUtility::GetLastFromVector<uint8_t>({ NFilterOutputs... })>;
