#pragma once

#include "Pipeline/Internal/PipelineFilter.h"
#include "Pipeline/Internal/ProxyFilterBase.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>
#include <tuple>

// Scaffolding that creates a gap in the data pipe by giving the input to a
// sink and getting output from a new source
template <uint8_t NInputs, uint8_t NOutputs>
class SpliceFilter : public ProxyFilterBase<NInputs, NOutputs>
{
	// Template implementation must be in the header file

public:
	SpliceFilter(PipelineSink<NInputs>* sink, PipelineSource<NOutputs>* source)
	{
		if (sink == nullptr)
		{
			throw std::invalid_argument("Sink cannot be nullptr");
		}

		if (source == nullptr)
		{
			throw std::invalid_argument("Source cannot be nullptr");
		}

		ProxyBase::components_ = { sink, source };

		// Initialize input pins
		TemplateUtility::For<NInputs>([&, this]<uint8_t i>()
		{
			this->template GetInputPin<i>().Initialize(this);
		});
		
		ProxySinkBase<NInputs>::onInputPinsConnected_ = [this, sink, source]()
			{
				// Connect input pins to sink
				TemplateUtility::For<NInputs>([this, sink]<uint8_t i>()
				{
					sink->template GetInputPin<i>().ConnectToOutputPin(this->template GetInputPin<i>().GetConnectedPin());
				});

				sink->OnInputPinsConnected();

				// Initialize output pins based on source
				TemplateUtility::For<NOutputs>([this, source]<uint8_t i>()
				{
					this->template GetOutputPin<i>().Initialize(this, source->template GetOutputPin<i>().GetFormat());
				});
			};

		ProxySourceBase<NOutputs>::pushOutputData_ = [this, source]()
			{
				// Push output data from the source's outputs
				TemplateUtility::For<NOutputs>([&, this]<uint8_t i>()
				{
					this->template GetOutputPin<i>().SetData(source->template GetOutputPin<i>().GetData());
					this->template GetOutputPin<i>().SetSize(source->template GetOutputPin<i>().GetSize());
				});
			};
	}

	virtual ~SpliceFilter() { }
};
