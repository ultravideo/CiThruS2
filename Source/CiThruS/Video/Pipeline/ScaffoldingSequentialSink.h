#pragma once

#include "PipelineSink.h"
#include "PipelineFilter.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>

// Scaffolding for sequentially connected sinks
class ImageSequentialSink : public PipelineSink<1>
{
	// Template implementation must be in the header file

public:
	template <uint8_t N>
	ImageSequentialSink(const ConstWrapper<PipelineFilter<1, 1>*> (&filters)[N], PipelineSink<1>* sink) : sink_(sink)
	{
		static_assert(N > 0);

		if (!sink_)
		{
			throw std::invalid_argument("Sink cannot be nullptr");
		}

		filters_.resize(N);

		for (int i = 0; i < N; i++)
		{
			filters_[i] = filters[i].Value;

			if (!filters_[i])
			{
				throw std::invalid_argument("Filter cannot be nullptr");
			}
		}

		GetInputPin<0>().AcceptAnyFormat();
	}

	~ImageSequentialSink()
	{
		for (int i = 0; i < filters_.size(); i++)
		{
			delete filters_[i];
		}

		filters_.clear();

		delete sink_;
		sink_ = nullptr;
	}

	virtual void Process() override
	{
		for (int i = 0; i < filters_.size(); i++)
		{
			filters_[i]->Process();
		}

		sink_->Process();
	}

	virtual void OnInputPinsConnected() override
	{
		filters_[0]->GetInputPin<0>().ConnectToOutputPin(GetInputPin<0>().GetConnectedPin());
		filters_[0]->OnInputPinsConnected();

		for (int i = 1; i < filters_.size(); i++)
		{
			filters_[i]->GetInputPin<0>().ConnectToOutputPin(filters_[i - 1]->GetOutputPin<0>());
			filters_[i]->OnInputPinsConnected();
		}

		sink_->GetInputPin<0>().ConnectToOutputPin(filters_[filters_.size() - 1]->GetOutputPin<0>());
		sink_->OnInputPinsConnected();
	}

protected:
	std::vector<PipelineFilter<1, 1>*> filters_;
	PipelineSink<1>* sink_;
};
