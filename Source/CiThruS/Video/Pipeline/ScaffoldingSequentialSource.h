#pragma once

#include "PipelineSource.h"
#include "PipelineFilter.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>

// Scaffolding for sequentially connected sources
class ImageSequentialSource : public PipelineSource<1>
{
	// Template implementation must be in the header file

public:
	template <uint8_t N>
	ImageSequentialSource(PipelineSource<1>* source, const ConstWrapper<PipelineFilter<1, 1>*> (&filters)[N]) : source_(source)
	{
		static_assert(N > 0);

		if (!source_)
		{
			throw std::invalid_argument("Source cannot be nullptr");
		}

		filters_.resize(N);

		for (int i = 0; i < N; i++)
		{
			if (!filters[i].Filter)
			{
				throw std::invalid_argument("Filter cannot be nullptr");
			}

			filters_[i] = filters[i].Filter;
		}

		filters_[0]->GetInputPin<0>().ConnectToOutputPin(source_[i - 1]->GetOutputPin<0>());
		filters_[0]->OnInputPinsConnected();

		for (int i = 1; i < N; i++)
		{
			filters_[i]->GetInputPin<0>().ConnectToOutputPin(filters_[i - 1]->GetOutputPin<0>());
			filters_[i]->OnInputPinsConnected();
		}

		GetOutputPin<0>().SetFormat(filters_[N - 1]->GetOutputPin<0>().GetFormat());
	}

	~ImageSequentialSource()
	{
		delete source_;
		source_ = nullptr;

		for (int i = 0; i < filters_.size(); i++)
		{
			delete filters_[i];
		}

		filters_.clear();
	}

	virtual void Process() override
	{
		source_->Process();

		for (int i = 0; i < filters_.size(); i++)
		{
			filters_[i]->Process();
		}

		GetOutputPin<0>().SetData(filters_[filters_.size() - 1]->GetOutputPin<0>().GetData());
		GetOutputPin<0>().SetSize(filters_[filters_.size() - 1]->GetOutputPin<0>().GetSize());
	}

protected:
	PipelineSource<1>* source_;
	std::vector<PipelineFilter<1, 1>*> filters_;
};
