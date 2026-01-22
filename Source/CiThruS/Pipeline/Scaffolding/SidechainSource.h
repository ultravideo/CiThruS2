#pragma once

#include "Pipeline/Internal/PipelineSource.h"
#include "Pipeline/Internal/PipelineFilter.h"
#include "Pipeline/Internal/ProxyFilterBase.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>

// Scaffolding for adding an independent source as a secondary input to a filter
template <uint8_t NInputs, uint8_t NOutputs>
class SidechainSource : public ProxyFilterBase<NInputs, NOutputs>
{
	// Template implementation must be in the header file

public:
	template <uint8_t NSourceOutputs>
	SidechainSource(PipelineSource<NSourceOutputs>* source, PipelineFilter<NSourceOutputs + NInputs, NOutputs>* filter)
	{
		if (!source)
		{
			throw std::invalid_argument("Source cannot be nullptr");
		}

		if (!filter)
		{
			throw std::invalid_argument("Filter cannot be nullptr");
		}

		// Initialize input pins
		TemplateUtility::For<NInputs>([&, this]<uint8_t i>()
		{
			this->template GetInputPin<i>().Initialize(this);
		});

		ProxyBase::components_.resize(2);

		ProxyBase::components_[0] = source;
		ProxyBase::components_[1] = filter;

		ProxySinkBase<NInputs>::onInputPinsConnected_ = [this, source, filter]()
			{
				// Connect all of the extra input pins to the filter
				TemplateUtility::For<NInputs>([&, this]<uint8_t i>()
				{
					filter->template GetInputPin<i>().ConnectToOutputPin(this->template GetInputPin<i>().GetConnectedPin());
				});

				// Then connect the rest to the source's output pins
				TemplateUtility::For<NSourceOutputs>([&, this]<uint8_t i>()
				{
					filter->template GetInputPin<i + NInputs>().ConnectToOutputPin(source->template GetOutputPin<i>());
				});

				filter->OnInputPinsConnected();

				// Initialize output pins based on the output pins of the filter
				TemplateUtility::For<NOutputs>([&, this]<uint8_t i>()
				{
					this->template GetOutputPin<i>().Initialize(this, filter->template GetOutputPin<i>().GetFormat());
				});
			};

		ProxySourceBase<NOutputs>::pushOutputData_ = [this, filter]()
			{
				// Push output data from the filter's output pins
				TemplateUtility::For<NOutputs>([&, this]<uint8_t i>()
				{
					this->template GetOutputPin<i>().SetData(filter->template GetOutputPin<i>().GetData());
					this->template GetOutputPin<i>().SetSize(filter->template GetOutputPin<i>().GetSize());
				});
			};
	}

	virtual ~SidechainSource() { }
};

// Deduction guide so that you don't have to specify NInputs and NOutputs manually when creating a new instance of this class
template <uint8_t NSourceOutputs, uint8_t NFilterInputs, uint8_t NFilterOutputs>
SidechainSource(PipelineSource<NSourceOutputs>*, PipelineFilter<NFilterInputs, NFilterOutputs>*) -> SidechainSource<NFilterInputs - NSourceOutputs, NFilterOutputs>;
