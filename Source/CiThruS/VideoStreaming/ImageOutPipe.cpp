#include "ImageOutPipe.h"
#include "IImageFilter.h"

ImageOutPipe::ImageOutPipe(IImageSource* source, std::vector<IImageFilter*> filters)
{
	components_ = std::vector<IImagePipelineComponent*>(filters.size());

	// Connect the pipeline components
	filters[0]->SetInput(source);

	for (int i = 0; i < filters.size() - 1; i++)
	{
		filters[i + 1]->SetInput(filters[i]);
	}

	// Put them into the same vector for convenience
	for (int i = 0; i < filters.size(); i++)
	{
		components_[i] = filters[i];
	}

	lastFilter_ = filters.back();
}

ImageOutPipe::~ImageOutPipe()
{
	for (IImagePipelineComponent* component : components_)
	{
		delete component;
	}

	components_.clear();
}

void ImageOutPipe::Process()
{
	for (IImagePipelineComponent* component : components_)
	{
		component->Process();
	}
}
