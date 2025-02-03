#pragma once

#include "IImageSink.h"
#include "IImageFork.h"
#include "IImageFilter.h"
#include "Misc/TemplateUtility.h"

#include <array>
#include <vector>
#include <stdexcept>

// Allows conventiently constructing a sink pipe using an initializer list
struct ImageSinkPipe
{
public:
	ImageSinkPipe() : Filters(), Sink(nullptr) { }
	ImageSinkPipe(std::vector<IImageFilter*> filters, IImageSink* sink)
		: Filters(filters), Sink(sink) { }
	ImageSinkPipe(IImageSink* sink)
		: Filters(), Sink(sink) { }

	std::vector<IImageFilter*> Filters;
	IImageSink* Sink;
};

// Manages parallel image pipes which start from the same fork but end at
// different sinks, enabling the whole layer to be treated as a sink by itself
template <uint8_t N>
class ImageParallelSink : public IImageSink
{
	// Template implementation must be in the header file

public:
	ImageParallelSink(IImageFork<N>* fork, const ImageSinkPipe (&pipes)[N]) : fork_(fork)
	{
		if (!fork_)
		{
			throw std::invalid_argument("Fork cannot be nullptr");
		}

		for (int i = 0; i < N; i++)
		{
			pipes_[i] = pipes[i];
		}
	}

	virtual ~ImageParallelSink()
	{
		delete fork_;
		fork_ = nullptr;

		for (int i = 0; i < N; i++)
		{
			for (int j = 0; j < pipes_[i].Filters.size(); j++)
			{
				delete pipes_[i].Filters[j];
			}

			pipes_[i].Filters.clear();

			delete pipes_[i].Sink;
		}
	}

	virtual void Process() override
	{
		fork_->Process();

		for (int i = 0; i < N; i++)
		{
			for (IImageFilter* const& filter : pipes_[i].Filters)
			{
				filter->Process();
			}

			pipes_[i].Sink->Process();
		}
	}

	virtual bool SetInput(const IImageSource* source) override
	{
		Connect(source, fork_);

		// Pipeline components should be connected in topological order to propagate data formats
		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			std::vector<IImageFilter*>& filters = pipes_[i].Filters;

			if (filters.size() == 0)
			{
				Connect(fork_->GetSourceAtIndex<i>(), pipes_[i].Sink);

				return;
			}

			Connect(fork_->GetSourceAtIndex<i>(), filters[0]);

			for (int j = 0; j < filters.size() - 1; j++)
			{
				Connect(filters[j], filters[j + 1]);
			}

			Connect(filters[filters.size() - 1], pipes_[i].Sink);
		});

		return true;
	}

protected:
	IImageFork<N>* fork_;
	std::array<ImageSinkPipe, N> pipes_;

	void Connect(const IImageSource* source, IImageSink* sink)
	{
		if (!sink->SetInput(source))
		{
			throw std::exception("Failed to connect pipeline components");
		}
	}
};
