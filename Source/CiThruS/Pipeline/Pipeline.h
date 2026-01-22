#pragma once

#include <vector>

#include "Pipeline/Internal/PipelineSource.h"
#include "Pipeline/Internal/PipelineFilter.h"
#include "Pipeline/Internal/PipelineSink.h"
#include "Pipeline/Scaffolding/SequentialFilter.h"

#include "Misc/TemplateUtility.h"

// Pipeline for processing data step by step
class CITHRUS_API Pipeline
{
	// Template implementations must be in the header file

public:
	template<uint8_t N, uint8_t M>
	Pipeline(PipelineSource<N>* source, PipelineFilter<N, M>* filter, PipelineSink<M>* sink)
	{
		if (!source)
		{
			throw std::invalid_argument("Source cannot be nullptr");
		}

		if (!filter)
		{
			throw std::invalid_argument("Filter cannot be nullptr");
		}

		if (!sink)
		{
			throw std::invalid_argument("Sink cannot be nullptr");
		}

		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			filter->template GetInputPin<i>().ConnectToOutputPin(source->template GetOutputPin<i>());
		});
		
		filter->OnInputPinsConnected();

		TemplateUtility::For<M>([&, this]<uint8_t i>()
		{
			sink->template GetInputPin<i>().ConnectToOutputPin(filter->template GetOutputPin<i>());
		});

		sink->OnInputPinsConnected();

		components_.resize(3);

		components_[0] = source;
		components_[1] = filter;
		components_[2] = sink;
	}

	template<uint8_t N>
	Pipeline(PipelineSource<N>* source, PipelineSink<N>* sink)
	{
		if (!source)
		{
			throw std::invalid_argument("Source cannot be nullptr");
		}

		if (!sink)
		{
			throw std::invalid_argument("Sink cannot be nullptr");
		}

		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			sink->template GetInputPin<i>().ConnectToOutputPin(source->template GetOutputPin<i>());
		});

		sink->OnInputPinsConnected();

		components_.resize(2);

		components_[0] = source;
		components_[1] = sink;
	}

	// Usage: new Pipeline(new Source(), new Filter1(), new Filter2(), new Sink());
	// 
	// The reason why the constructor implementation is weird is because C++
	// doesn't like having function parameters after a parameter pack, so both
	// the filters and the sink have to be in the same parameter pack if we
	// want the sink to be last in the constructor call
	template <typename... T, typename = std::enable_if_t<(sizeof...(T) > 3)>()>
	Pipeline(T*... sourceFirstThenFiltersAndSinkLast)
	{
		auto filtersTuple = std::tie(sourceFirstThenFiltersAndSinkLast...);
		auto source = std::get<0>(filtersTuple);
		auto sink = std::get<sizeof...(sourceFirstThenFiltersAndSinkLast) - 1>(filtersTuple);

		UnrollComponents(source, sink, std::forward_as_tuple(sourceFirstThenFiltersAndSinkLast...), std::make_index_sequence<sizeof...(sourceFirstThenFiltersAndSinkLast) - 2>());
	}

	virtual ~Pipeline();

	void Run();

protected:
	std::vector<IPipelineComponent*> components_;

	template <uint8_t Q, uint8_t R, uint8_t... O, uint8_t... P>
	void ConstructInternal(PipelineSource<Q>* source, PipelineSink<R>* sink, PipelineFilter<O, P>*... filters)
	{
		static_assert(std::is_same<std::integer_sequence<uint8_t, O..., R>, std::integer_sequence<uint8_t, Q, P...>>::value);

		if (!source)
		{
			throw std::invalid_argument("Source cannot be nullptr");
		}

		if (((filters == nullptr) || ...))
		{
			throw std::invalid_argument("Filter cannot be nullptr");
		}

		if (!sink)
		{
			throw std::invalid_argument("Sink cannot be nullptr");
		}

		auto filter = new SequentialFilter(filters...);

		TemplateUtility::For<Q>([&, this]<uint8_t i>()
		{
			filter->template GetInputPin<i>().ConnectToOutputPin(source->template GetOutputPin<i>());
		});

		filter->OnInputPinsConnected();

		TemplateUtility::For<R>([&, this]<uint8_t i>()
		{
			sink->template GetInputPin<i>().ConnectToOutputPin(filter->template GetOutputPin<i>());
		});

		sink->OnInputPinsConnected();

		components_.resize(3);

		components_[0] = source;
		components_[1] = filter;
		components_[2] = sink;
	}

	template <class T, uint8_t Q, uint8_t R, std::size_t... I>
	void UnrollComponents(PipelineSource<Q>* source, PipelineSink<R>* sink, T&& tuple, std::index_sequence<I...>)
	{
		ConstructInternal(source, sink, std::get<I + 1>(tuple)...);
	}
};
