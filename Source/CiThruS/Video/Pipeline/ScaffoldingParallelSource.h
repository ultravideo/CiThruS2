#pragma once

#include "PipelineSource.h"
#include "Misc/TemplateUtility.h"

#include <array>
#include <vector>
#include <stdexcept>

// Scaffolding for a row of parallel sources
template <uint8_t N>
class ScaffoldingParallelSource : public PipelineSource<N>
{
	// Template implementation must be in the header file

public:
	ScaffoldingParallelSource(const ConstWrapper<PipelineSink<1>*> (&sources)[N])
	{
		for (int i = 0; i < N; i++)
		{
			sources_[i] = sources[i].Value;

			if (!sources_[i])
			{
				throw std::invalid_argument("Source cannot be nullptr");
			}

			PipelineSource<N>::outputPins_[i].SetFormat(sources_[i]->template GetOutputPin<0>().GetFormat());
		}
	}

	~ScaffoldingParallelSource()
	{
		for (int i = 0; i < N; i++)
		{
			delete sources_[i];
		}
	}

	virtual void Process() override
	{
		for (int i = 0; i < N; i++)
		{
			sources_[i]->Process();
			PipelineSource<N>::outputPins_[i].SetData(sources_[i]->template GetOutputPin<0>().GetData());
			PipelineSource<N>::outputPins_[i].SetSize(sources_[i]->template GetOutputPin<0>().GetSize());
		}
	}

protected:
	std::array<PipelineSource<1>*, N> sources_;
};
