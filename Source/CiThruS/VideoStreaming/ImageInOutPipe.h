#pragma once

#include "IImageFilter.h"

#include <vector>

// Image pipe with both input data and output data
class CITHRUS_API ImageInOutPipe : public IImageFilter
{
public:
	ImageInOutPipe(std::vector<IImageFilter*> filters);
	virtual ~ImageInOutPipe();

	void Process();

	inline virtual void SetInput(const IImageSource* source) override
	{
		firstFilter_->SetInput(source);
	}
	inline virtual std::string GetInputFormat() const override { return firstFilter_->GetInputFormat(); }

	inline virtual uint8_t* const* GetOutput() const override { return lastFilter_->GetOutput(); }
	inline virtual const uint32_t* GetOutputSize() const override { return lastFilter_->GetOutputSize(); }
	inline virtual std::string GetOutputFormat() const override { return lastFilter_->GetOutputFormat(); }

protected:
	std::vector<IImagePipelineComponent*> components_;

	IImageSink* firstFilter_;
	IImageSource* lastFilter_;

	void Reconnect();
};
