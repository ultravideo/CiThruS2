#pragma once

#include "PipelineFilter.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>

// Scaffolding for sequentially connected filters
class ImageSequentialFilter : public PipelineFilter<1, 1>
{
	// Template implementation must be in the header file

public:
	template <uint8_t N>
	ImageSequentialFilter(const ConstWrapper<PipelineFilter<1, 1>*> (&filters)[N])
	{
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

	ImageSequentialFilter()
	{
		GetInputPin<0>().AcceptAnyFormat();
	}

	~ImageSequentialFilter()
	{
		for (int i = 0; i < filters_.size(); i++)
		{
			delete filters_[i];
		}
	}

	virtual void Process() override
	{
		for (int i = 0; i < filters_.size(); i++)
		{
			filters_[i]->Process();
		}

		if (filters_.size() <= 0)
		{
			GetOutputPin<0>().SetData(GetInputPin<0>().GetConnectedPin().GetData());
			GetOutputPin<0>().SetSize(GetInputPin<0>().GetConnectedPin().GetSize());
		}
		else
		{
			GetOutputPin<0>().SetData(filters_[filters_.size() - 1]->GetOutputPin<0>().GetData());
			GetOutputPin<0>().SetSize(filters_[filters_.size() - 1]->GetOutputPin<0>().GetSize());
		}
	}

	virtual void OnInputPinsConnected() override
	{
		if (filters_.size() <= 0)
		{
			GetOutputPin<0>().SetFormat(GetInputPin<0>().GetConnectedPin().GetFormat());

			return;
		}

		filters_[0]->GetInputPin<0>().ConnectToOutputPin(GetInputPin<0>().GetConnectedPin());
		filters_[0]->OnInputPinsConnected();

		for (int i = 1; i < filters_.size(); i++)
		{
			filters_[i]->GetInputPin<0>().ConnectToOutputPin(filters_[i - 1]->GetOutputPin<0>());
			filters_[i]->OnInputPinsConnected();
		}

		GetOutputPin<0>().SetFormat(filters_[filters_.size() - 1]->GetOutputPin<0>().GetFormat());
	}

protected:
	std::vector<PipelineFilter<1, 1>*> filters_;
};
