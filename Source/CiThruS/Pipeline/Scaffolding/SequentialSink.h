#pragma once

#include "Pipeline/Internal/PipelineFilter.h"
#include "Pipeline/Internal/ProxySinkBase.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>
#include <tuple>

// Scaffolding for sequentially connecting filters to a sink
template <uint8_t NInputs>
class SequentialSink : public ProxySinkBase<NInputs>
{
	// Template implementation must be in the header file

public:
	// Usage: new SequentialSink(new Filter1(), new Filter2(), new Sink());
	// 
	// The reason why the constructor implementation is weird is because C++
	// doesn't like having function parameters after a parameter pack, so both
	// the filters and the sink have to be in the same parameter pack if we
	// want the sink to be last in the constructor call
	template <class... TComponents>
	SequentialSink(TComponents*... filtersFirstAndSinkLast)
	{
		auto filtersTuple = std::tie(filtersFirstAndSinkLast...);
		auto sink = std::get<sizeof...(filtersFirstAndSinkLast) - 1>(filtersTuple);

		UnrollComponents(sink, std::forward_as_tuple(filtersFirstAndSinkLast...), std::make_index_sequence<sizeof...(filtersFirstAndSinkLast) - 1>());
	}

	virtual ~SequentialSink() { }

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

	template <uint8_t NSinkInputs, uint8_t... NFilterInputs, uint8_t... NFilterOutputs>
	void ConstructInternal(PipelineSink<NSinkInputs>* sink, PipelineFilter<NFilterInputs, NFilterOutputs>*... filters)
	{
		static_assert(std::is_same<std::integer_sequence<uint8_t, NFilterInputs..., NSinkInputs>, std::integer_sequence<uint8_t, NInputs, NFilterOutputs...>>::value,
			"The output count of each component must match the input count of the next component");
		static_assert(sizeof...(filters) > 0,
			"ScaffoldingSequentialSink is meant for connecting at least one filter to a sink. "
			"If you don't need any filters, you should be able to use the sink by itself without any scaffolding");

		if (((filters == nullptr) || ...))
		{
			throw std::invalid_argument("Filter cannot be nullptr");
		}

		if (sink == nullptr)
		{
			throw std::invalid_argument("Sink cannot be nullptr");
		}

		ProxyBase::components_ = { filters..., sink };

		// Initialize input pins
		TemplateUtility::For<NInputs>([&, this]<uint8_t i>()
		{
			this->template GetInputPin<i>().Initialize(this);
		});

		ProxySinkBase<NInputs>::onInputPinsConnected_ = [this, filters..., sink]()
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

				// Connect last filter to sink
				TemplateUtility::For<NSinkInputs>([&]<uint8_t i>()
				{
					sink->template GetInputPin<i>().ConnectToOutputPin(lastFilter->template GetOutputPin<i>());
				});

				sink->OnInputPinsConnected();
			};
	}

	template <class TTuple, int NSinkInputs, std::size_t... I>
	void UnrollComponents(PipelineSink<NSinkInputs>* sink, TTuple&& tuple, std::index_sequence<I...>)
	{
		ConstructInternal(sink, (std::get<I>(tuple), ...));
	}
};

// Deduction guide so that you don't have to specify N manually when creating a new instance of this class
template <class... TOthers, uint8_t NFilterInputs, uint8_t NFilterOutputs>
SequentialSink(PipelineFilter<NFilterInputs, NFilterOutputs>*, TOthers*...) -> SequentialSink<NFilterInputs>;
