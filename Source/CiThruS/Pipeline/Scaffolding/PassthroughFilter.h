#pragma once

#include "Pipeline/Internal/PipelineFilter.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>
#include <functional>

// Scaffolding for passing the data through unmodified
template <uint8_t NInputsAndOutputs>
class PassthroughFilter : public PipelineFilter<NInputsAndOutputs, NInputsAndOutputs>
{
	// Template implementation must be in the header file

public:
	PassthroughFilter()
	{
		TemplateUtility::For<NInputsAndOutputs>([&, this]<uint8_t i>()
		{
			this->template GetInputPin<i>().Initialize(this);
		});
	}

	virtual ~PassthroughFilter() { }

	virtual void Process() override
	{
		TemplateUtility::For<NInputsAndOutputs>([&, this]<uint8_t i>()
		{
			this->template GetOutputPin<i>().SetData(this->template GetInputPin<i>().GetConnectedPin().GetData());
			this->template GetOutputPin<i>().SetSize(this->template GetInputPin<i>().GetConnectedPin().GetSize());
		});
	}

	virtual void OnInputPinsConnected() override
	{
		TemplateUtility::For<NInputsAndOutputs>([&, this]<uint8_t i>()
		{
			this->template GetOutputPin<i>().Initialize(this, this->template GetInputPin<i>().GetConnectedPin().GetFormat());
		});
	}
};
