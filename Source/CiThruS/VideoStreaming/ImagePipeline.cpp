#include "ImagePipeline.h"
#include "IImageSource.h"
#include "IImageFilter.h"
#include "IImageSink.h"
#include "IImagePipelineComponent.h"

ImagePipeline::ImagePipeline(IImageSource* source, std::vector<IImageFilter*> filters, IImageSink* sink)
{
	components_ = std::vector<IImagePipelineComponent*>(filters.size() + 2);

	// Connect the pipeline components
	filters[0]->SetInput(source);

	for (int i = 0; i < filters.size() - 1; i++)
	{
		filters[i + 1]->SetInput(filters[i]);
	}

	sink->SetInput(filters[filters.size() - 1]);

	// Put them into the same vector for convenience
	components_[0] = source;

	for (int i = 0; i < filters.size(); i++)
	{
		components_[i + 1] = filters[i];
	}

	components_[components_.size() - 1] = sink;
}

ImagePipeline::~ImagePipeline()
{
	for (IImagePipelineComponent* component : components_)
	{
		delete component;
	}

	components_.clear();
}

void ImagePipeline::Process()
{
	for (IImagePipelineComponent* component : components_)
	{
		component->Process();
	}
}
