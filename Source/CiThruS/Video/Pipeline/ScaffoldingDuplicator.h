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
		this->template GetInputPin<0>().AcceptAnyFormat();
	}

	virtual void Process() override
	{
		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			this->template GetOutputPin<i>().SetData(this->template GetInputPin<0>().GetConnectedPin().GetData());
			this->template GetOutputPin<i>().SetSize(this->template GetInputPin<0>().GetConnectedPin().GetSize());
		});
	}

	virtual void OnInputPinsConnected() override
	{
		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			this->template GetOutputPin<i>().SetFormat(this->template GetInputPin<0>().GetConnectedPin().GetFormat());
		});
	}
};
