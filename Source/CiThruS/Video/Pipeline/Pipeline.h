#pragma once

#include <vector>

#include "PipelineSource.h"
#include "PipelineFilter.h"
#include "PipelineSink.h"
#include "Misc/TemplateUtility.h"

// Pipeline for processing data step by step
class CITHRUS_API Pipeline
{
public:
	template<uint8_t N, uint8_t M, uint8_t O>
	Pipeline(PipelineSource<O>* source, PipelineFilter<N, M>* filter, PipelineSink<M>* sink)
	{
		static_assert(O >= N);

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

	virtual ~Pipeline();

	void Run();

protected:
	std::vector<IPipelineComponent*> components_;
};
