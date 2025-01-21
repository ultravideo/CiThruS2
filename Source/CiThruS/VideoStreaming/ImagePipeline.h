#pragma once

#include <vector>

class IImageSource;
class IImageFilter;
class IImageSink;
class IImagePipelineComponent;

// Pipeline of image filters for conveniently processing image data step by step
class CITHRUS_API ImagePipeline
{
public:
	ImagePipeline(IImageSource* source, std::vector<IImageFilter*> filters, IImageSink* sink);
	virtual ~ImagePipeline();

	void Process();

protected:
	std::vector<IImagePipelineComponent*> components_;
};
