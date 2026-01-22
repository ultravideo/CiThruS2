#pragma once

#include "Pipeline/Internal/PipelineFilter.h"
#include "Misc/TemplateUtility.h"

// Scaffolding for duplicating input data from one pin to multiple pins
template <uint8_t NOutputs>
class DuplicatorFilter : public PipelineFilter<1, NOutputs>
{
	// Template implementation must be in the header file

public:
	DuplicatorFilter()
	{
		this->template GetInputPin<0>().Initialize(this);
	}

	virtual ~DuplicatorFilter() { }

	virtual void Process() override
	{
		TemplateUtility::For<NOutputs>([&, this]<uint8_t i>()
		{
			this->template GetOutputPin<i>().SetData(this->template GetInputPin<0>().GetConnectedPin().GetData());
			this->template GetOutputPin<i>().SetSize(this->template GetInputPin<0>().GetConnectedPin().GetSize());
		});
	}

	virtual void OnInputPinsConnected() override
	{
		TemplateUtility::For<NOutputs>([&, this]<uint8_t i>()
		{
			this->template GetOutputPin<i>().Initialize(this, this->template GetInputPin<0>().GetConnectedPin().GetFormat());
		});
	}
};
