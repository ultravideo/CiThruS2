#pragma once

#include "PipelineFilter.h"
#include "Misc/TemplateUtility.h"

// Duplicates the input data from one pin to multiple pins
template <uint8_t N>
class ScaffoldingDuplicator : public PipelineFilter<1, N>
{
	// Template implementation must be in the header file

public:
	ScaffoldingDuplicator()
	{
		this->GetInputPin<0>().AcceptAnyFormat();
	}

	virtual void Process() override
	{
		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			this->GetOutputPin<i>().SetData(this->GetInputPin<0>().GetConnectedPin().GetData());
			this->GetOutputPin<i>().SetSize(this->GetInputPin<0>().GetConnectedPin().GetSize());
		});
	}

	virtual void OnInputPinsConnected() override
	{
		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			this->GetOutputPin<i>().SetFormat(this->GetInputPin<0>().GetConnectedPin().GetFormat());
		});
	}
};
