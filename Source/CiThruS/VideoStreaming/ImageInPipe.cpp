#include "ImageInPipe.h"
#include "IImageFilter.h"

ImageInPipe::ImageInPipe(std::vector<IImageFilter*> filters, IImageSink* sink)
{
	components_ = std::vector<IImagePipelineComponent*>(filters.size());

	// Connect the pipeline components
	for (int i = 0; i < filters.size() - 1; i++)
	{
		filters[i + 1]->SetInput(filters[i]);
	}

	sink->SetInput(filters[filters.size() - 1]);

	// Put them into the same vector for convenience
	for (int i = 0; i < filters.size(); i++)
	{
		components_[i] = filters[i];
	}

	firstFilter_ = filters.front();
}

ImageInPipe::~ImageInPipe()
{
	for (IImagePipelineComponent* component : components_)
	{
		delete component;
	}

	components_.clear();
}

void ImageInPipe::Process()
{
	for (IImagePipelineComponent* component : components_)
	{
		component->Process();
	}
}
