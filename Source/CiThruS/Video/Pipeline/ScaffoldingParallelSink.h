#pragma once

#include "PipelineSink.h"
#include "Misc/TemplateUtility.h"

#include <array>
#include <vector>
#include <stdexcept>

// Scaffolding for a row of parallel sinks
template <uint8_t N>
class ScaffoldingParallelSink : public PipelineSink<N>
{
	// Template implementation must be in the header file

public:
	ScaffoldingParallelSink(const ConstWrapper<PipelineSink<1>*> (&sinks)[N])
	{
		for (int i = 0; i < N; i++)
		{
			sinks_[i] = sinks[i].Value;

			if (!sinks_[i])
			{
				throw std::invalid_argument("Sink cannot be nullptr");
			}
		}

		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			this->template GetInputPin<i>().AcceptAnyFormat();
		});
	}

	~ScaffoldingParallelSink()
	{
		for (int i = 0; i < N; i++)
		{
			delete sinks_[i];
		}
	}

	virtual void Process() override
	{
		for (int i = 0; i < N; i++)
		{
			sinks_[i]->Process();
		}
	}

	virtual void OnInputPinsConnected() override
	{
		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			sinks_[i]->template GetInputPin<0>().ConnectToOutputPin(this->template GetInputPin<i>().GetConnectedPin());
			sinks_[i]->OnInputPinsConnected();
		});
	}

protected:
	std::array<PipelineSink<1>*, N> sinks_;
};
