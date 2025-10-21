#pragma once

#include "PipelineFilter.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>

// Scaffolding for a row of parallel filters
template <uint8_t N>
class ScaffoldingParallelFilter : public PipelineFilter<N, N>
{
	// Template implementation must be in the header file

public:
	ScaffoldingParallelFilter(const ConstWrapper<PipelineFilter<1, 1>*> (&filters)[N])
	{
		for (int i = 0; i < N; i++)
		{
			filters_[i] = filters[i].Value;

			if (!filters_[i])
			{
				throw std::invalid_argument("Filter cannot be nullptr");
			}
		}

		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			this->template GetInputPin<i>().AcceptAnyFormat();
		});
	}

	~ScaffoldingParallelFilter()
	{
		for (int i = 0; i < N; i++)
		{
			delete filters_[i];
		}
	}

	virtual void Process() override
	{
		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			filters_[i]->Process();
			this->template GetOutputPin<i>().SetData(filters_[i]->template GetOutputPin<0>().GetData());
			this->template GetOutputPin<i>().SetSize(filters_[i]->template GetOutputPin<0>().GetSize());
		});
	}

	virtual void OnInputPinsConnected() override
	{
		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			filters_[i]->template GetInputPin<0>().ConnectToOutputPin(this->template GetInputPin<i>().GetConnectedPin());
			filters_[i]->OnInputPinsConnected();
			this->template GetOutputPin<i>().SetFormat(filters_[i]->template GetOutputPin<0>().GetFormat());
		});
	}

protected:
	std::array<PipelineFilter<1, 1>*, N> filters_;
};
