#pragma once

#include "IImageSink.h"

#include <vector>

class IImageFilter;

// Image pipe with only input data
class CITHRUS_API ImageInPipe : public IImageSink
{
public:
	ImageInPipe(std::vector<IImageFilter*> filters, IImageSink* sink);
	virtual ~ImageInPipe();

	void Process();

	inline virtual void SetInput(const IImageSource* source) override
	{
		firstFilter_->SetInput(source);
	}
	inline virtual std::string GetInputFormat() const override { return firstFilter_->GetInputFormat(); }

protected:
	std::vector<IImagePipelineComponent*> components_;

	IImageSink* firstFilter_;
};
