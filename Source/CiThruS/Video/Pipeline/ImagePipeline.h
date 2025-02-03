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
	ImagePipeline(IImageSource* source, IImageSink* sink);
	virtual ~ImagePipeline();

	void Run();

protected:
	IImageSource* source_;
	std::vector<IImageFilter*> filters_;
	IImageSink* sink_;

	void Connect(const IImageSource* source, IImageSink* sink);
};
