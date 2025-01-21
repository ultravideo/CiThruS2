#include "ImageInOutPipe.h"

ImageInOutPipe::ImageInOutPipe(std::vector<IImageFilter*> filters)
{
	components_ = std::vector<IImagePipelineComponent*>(filters.size());

	// Connect the pipeline components
	for (int i = 0; i < filters.size() - 1; i++)
	{
		filters[i + 1]->SetInput(filters[i]);
	}

	// Put them into the same vector for convenience
	for (int i = 0; i < filters.size(); i++)
	{
		components_[i] = filters[i];
	}

	firstFilter_ = filters.front();
	lastFilter_ = filters.back();
}

ImageInOutPipe::~ImageInOutPipe()
{
	for (IImagePipelineComponent* component : components_)
	{
		delete component;
	}

	components_.clear();
}

void ImageInOutPipe::Process()
{
	for (IImagePipelineComponent* component : components_)
	{
		component->Process();
	}
}
