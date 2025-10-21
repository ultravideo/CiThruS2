#pragma once

#include "PipelineFilter.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>

// Scaffolding for adding an independent source as a secondary input to a filter
template <uint8_t N, uint8_t M>
class ScaffoldingSidechainSource : public PipelineFilter<N, M>
{
	// Template implementation must be in the header file

public:
	template <uint8_t O>
	ScaffoldingSidechainSource(PipelineSource<O>* source, PipelineFilter<O + N, M>* filter)
	{
		if (!source)
		{
			throw std::invalid_argument("Source cannot be nullptr");
		}

		if (!filter)
		{
			throw std::invalid_argument("Filter cannot be nullptr");
		}

		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			this->template GetInputPin<i>().AcceptAnyFormat();
		});

		components_.resize(2);

		components_[0] = source;
		components_[1] = filter;

		// Saving these functions like this removes the need for the user to
		// explicitly specify O when using this class
		pushOutputData_ = [this, filter]()
			{
				TemplateUtility::For<M>([&, this]<uint8_t i>()
				{
					this->template GetOutputPin<i>().SetData(filter->template GetOutputPin<i>().GetData());
					this->template GetOutputPin<i>().SetSize(filter->template GetOutputPin<i>().GetSize());
				});
			};

		onInputPinsConnected_ = [this, source, filter]()
			{
				TemplateUtility::For<N>([&, this]<uint8_t i>()
				{
					filter->template GetInputPin<i>().ConnectToOutputPin(this->template GetInputPin<i>().GetConnectedPin());
				});

				TemplateUtility::For<O>([&, this]<uint8_t i>()
				{
					filter->template GetInputPin<i + N>().ConnectToOutputPin(source->template GetOutputPin<i>());
				});

				filter->OnInputPinsConnected();

				TemplateUtility::For<M>([&, this]<uint8_t i>()
				{
					this->template GetOutputPin<i>().SetFormat(filter->template GetOutputPin<i>().GetFormat());
				});
			};
	}

	~ScaffoldingSidechainSource()
	{
		for (int i = 0; i < components_.size(); i++)
		{
			delete components_[i];
		}

		components_.clear();
	}

	virtual void Process() override
	{
		for (int i = 0; i < components_.size(); i++)
		{
			components_[i]->Process();
		}

		pushOutputData_();
	}

	virtual void OnInputPinsConnected() override
	{
		onInputPinsConnected_();
	}

protected:
	std::vector<IPipelineComponent*> components_;

	std::function<void()> pushOutputData_;
	std::function<void()> onInputPinsConnected_;
};
