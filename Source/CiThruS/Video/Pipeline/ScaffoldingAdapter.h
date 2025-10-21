#pragma once

#include "PipelineFilter.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>
#include <functional>

template <uint8_t N, uint8_t M>
class ScaffoldingAdapter : public PipelineFilter<N, M>
{
	// Template implementation must be in the header file

public:
	template <uint8_t O>
	ScaffoldingAdapter(PipelineFilter<N, O>* firstFilter, PipelineFilter<O, M>* secondFilter)
	{
		if (!firstFilter)
		{
			throw std::invalid_argument("Filter cannot be nullptr");
		}

		if (!secondFilter)
		{
			throw std::invalid_argument("Filter cannot be nullptr");
		}

		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			this->template GetInputPin<i>().AcceptAnyFormat();
		});

		components_.resize(2);

		components_[0] = firstFilter;
		components_[1] = secondFilter;

		// Saving these functions like this removes the need for the user to
		// explicitly specify O when using this class
		pushOutputData_ = [this, secondFilter]()
			{
				TemplateUtility::For<M>([&, this]<uint8_t i>()
				{
					this->template GetOutputPin<i>().SetData(secondFilter->template GetOutputPin<i>().GetData());
					this->template GetOutputPin<i>().SetSize(secondFilter->template GetOutputPin<i>().GetSize());
				});
			};

		onInputPinsConnected_ = [this, firstFilter, secondFilter]()
			{
				TemplateUtility::For<N>([&, this]<uint8_t i>()
				{
					firstFilter->template GetInputPin<i>().ConnectToOutputPin(this->template GetInputPin<i>().GetConnectedPin());
				});

				firstFilter->OnInputPinsConnected();

				TemplateUtility::For<O>([&, this]<uint8_t i>()
				{
					secondFilter->template GetInputPin<i>().ConnectToOutputPin(firstFilter->template GetOutputPin<i>());
				});

				secondFilter->OnInputPinsConnected();

				TemplateUtility::For<M>([&, this]<uint8_t i>()
				{
					this->template GetOutputPin<i>().SetFormat(secondFilter->template GetOutputPin<i>().GetFormat());
				});
			};
	}

	~ScaffoldingAdapter()
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
