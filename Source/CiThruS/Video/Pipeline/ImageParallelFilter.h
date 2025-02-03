#pragma once

#include "IImageMultiSink.h"
#include "IImageMultiSource.h"
#include "IImageFork.h"
#include "IImageFunnel.h"
#include "IImageFilter.h"
#include "Misc/TemplateUtility.h"

#include <vector>
#include <array>
#include <stdexcept>

// Manages parallel image pipes which start from the same fork and end at the
// same funnel, enabling the whole layer to be treated as a filter by itself
template <uint8_t N>
class ImageParallelFilter : public IImageFilter
{
	// Template implementation must be in the header file

public:
	ImageParallelFilter(IImageFork<N>* fork, const std::vector<IImageFilter*> (&pipes)[N], IImageFunnel<N>* funnel) : fork_(fork), funnel_(funnel)
	{
		if (!funnel_)
		{
			throw std::invalid_argument("Funnel cannot be nullptr");
		}

		for (int i = 0; i < N; i++)
		{
			pipes_[i] = pipes[i];

			if (pipes_[i].size() == 0)
			{
				if (!fork_)
				{
					throw std::invalid_argument("Pipe vector cannot be empty if fork is nullptr");
				}
			}
		}
	}

	ImageParallelFilter(const std::vector<IImageFilter*> (&pipes)[N], IImageFunnel<N>* funnel) : ImageParallelFilter(nullptr, pipes, funnel) { }
	ImageParallelFilter(IImageFork<N>* fork, IImageFunnel<N>* funnel) : ImageParallelFilter(fork, { }, funnel) { }

	virtual ~ImageParallelFilter()
	{
		delete fork_;
		fork_ = nullptr;

		for (int i = 0; i < N; i++)
		{
			for (int j = 0; j < pipes_[i].size(); j++)
			{
				delete pipes_[i][j];
			}

			pipes_[i].clear();
		}

		delete funnel_;
		funnel_ = nullptr;
	}

	virtual void Process() override
	{
		if (fork_)
		{
			fork_->Process();
		}

		for (int i = 0; i < N; i++)
		{
			for (int j = 0; j < pipes_[i].size(); j++)
			{
				pipes_[i][j]->Process();
			}
		}

		funnel_->Process();
	}

	virtual bool SetInput(const IImageSource* source) override
	{
		if (fork_)
		{
			Connect(source, fork_);

			return true;
		}

		for (int i = 0; i < N; i++)
		{
			Connect(source, pipes_[i][0]);
		}

		// Pipeline components should be connected in topological order to propagate data formats
		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			if (pipes_[i].size() == 0)
			{
				Connect(fork_->GetSourceAtIndex<i>(), funnel_->GetSinkAtIndex<i>());

				return;
			}

			if (fork_)
			{
				Connect(fork_->GetSourceAtIndex<i>(), pipes_[i][0]);
			}

			for (int j = 0; j < pipes_[i].size() - 1; j++)
			{
				Connect(pipes_[i][j], pipes_[i][j + 1]);
			}

			Connect(pipes_[i][pipes_[i].size() - 1], funnel_->GetSinkAtIndex<i>());
		});

		return true;
	}

	virtual uint8_t* const* GetOutput() const override { return funnel_->GetOutput(); }
	virtual const uint32_t* GetOutputSize() const override { return funnel_->GetOutputSize(); }
	virtual std::string GetOutputFormat() const override { return funnel_->GetOutputFormat(); }

protected:
	IImageFork<N>* fork_;
	std::array<std::vector<IImageFilter*>, N> pipes_;
	IImageFunnel<N>* funnel_;

	void Connect(const IImageSource* source, IImageSink* sink)
	{
		if (!sink->SetInput(source))
		{
			throw std::exception("Failed to connect pipeline components");
		}
	}
};
