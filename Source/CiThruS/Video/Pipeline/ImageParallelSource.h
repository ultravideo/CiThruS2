#pragma once

#include "IImageSource.h"
#include "IImageFilter.h"
#include "IImageFunnel.h"
#include "Misc/TemplateUtility.h"

#include <array>
#include <vector>
#include <stdexcept>

// Allows conventiently constructing a source pipe using an initializer list
struct ImageSourcePipe
{
public:
	ImageSourcePipe() : Source(nullptr), Filters() { }
	ImageSourcePipe(IImageSource* source, std::vector<IImageFilter*> filters)
		: Source(source), Filters(filters) { }
	ImageSourcePipe(IImageSource* source)
		: Source(source), Filters() { }

	IImageSource* Source;
	std::vector<IImageFilter*> Filters;
};

// Manages parallel image pipes which start from different sources but end at
// the same funnel, enabling the whole layer to be treated as a source by itself
template <uint8_t N>
class ImageParallelSource : public IImageSource
{
	// Template implementation must be in the header file

public:
	ImageParallelSource(const ImageSourcePipe (&pipes)[N], IImageFunnel<N>* funnel) : funnel_(funnel)
	{
		if (!funnel_)
		{
			throw std::invalid_argument("Funnel cannot be nullptr");
		}

		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			pipes_[i] = pipes[i];

			std::vector<IImageFilter*>& filters = pipes_[i].Filters;

			if (filters.size() == 0)
			{
				Connect(pipes_[i].Source, funnel_->GetSinkAtIndex<i>());

				return;
			}

			Connect(pipes_[i].Source, filters[0]);

			for (int j = 0; j < filters.size() - 1; j++)
			{
				Connect(filters[j], filters[j + 1]);
			}

			Connect(filters[filters.size() - 1], funnel_->GetSinkAtIndex<i>());
		});
	}

	virtual ~ImageParallelSource()
	{
		for (int i = 0; i < N; i++)
		{
			delete pipes_[i].Source;

			for (int j = 0; j < pipes_[i].Filters.size(); j++)
			{
				delete pipes_[i].Filters[j];
			}

			pipes_[i].Filters.clear();
		}

		delete funnel_;
		funnel_ = nullptr;
	}

	virtual void Process() override
	{
		for (int i = 0; i < N; i++)
		{
			pipes_[i].Source->Process();

			for (IImageFilter* const& filter : pipes_[i].Filters)
			{
				filter->Process();
			}
		}

		funnel_->Process();
	}

	virtual uint8_t* const* GetOutput() const override { return funnel_->GetOutput(); }
	virtual const uint32_t* GetOutputSize() const override { return funnel_->GetOutputSize(); }
	virtual std::string GetOutputFormat() const override { return funnel_->GetOutputFormat(); }

protected:
	std::array<ImageSourcePipe, N> pipes_;
	IImageFunnel<N>* funnel_;

	void Connect(const IImageSource* source, IImageSink* sink)
	{
		if (!sink->SetInput(source))
		{
			throw std::exception("Failed to connect pipeline components");
		}
	}
};
