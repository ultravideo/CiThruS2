#include "ImagePipeline.h"
#include "IImageSource.h"
#include "IImageFilter.h"
#include "IImageSink.h"
#include "IImagePipelineComponent.h"

ImagePipeline::ImagePipeline(IImageSource* source, std::vector<IImageFilter*> filters, IImageSink* sink)
	: source_(source), filters_(filters), sink_(sink)
{
	if (filters_.size() == 0)
	{
		Connect(source_, sink_);

		return;
	}

	Connect(source_, filters_[0]);

	for (int i = 0; i < filters_.size() - 1; i++)
	{
		Connect(filters_[i], filters_[i + 1]);
	}

	Connect(filters_[filters_.size() - 1], sink_);
}

ImagePipeline::ImagePipeline(IImageSource* source, IImageSink* sink) : ImagePipeline(source, { }, sink)
{

}

ImagePipeline::~ImagePipeline()
{
	delete source_;
	source_ = nullptr;

	for (int i = 0; i < filters_.size(); i++)
	{
		delete filters_[i];
	}

	filters_.clear();

	delete sink_;
	sink_ = nullptr;
}

void ImagePipeline::Run()
{
	source_->Process();

	for (int i = 0; i < filters_.size(); i++)
	{
		filters_[i]->Process();
	}

	sink_->Process();
}

void ImagePipeline::Connect(const IImageSource* source, IImageSink* sink)
{
	if (!sink->SetInput(source))
	{
		throw std::exception("Failed to connect pipeline components");
	}
}
