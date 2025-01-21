#pragma once

#include "IImageSource.h"

#include <vector>

class IImageFilter;

// Image pipe with only output data
class CITHRUS_API ImageOutPipe : public IImageSource
{
public:
	ImageOutPipe(IImageSource* source, std::vector<IImageFilter*> filters);
	virtual ~ImageOutPipe();

	void Process();

	inline virtual uint8_t* const* GetOutput() const override { return lastFilter_->GetOutput(); }
	inline virtual const uint32_t* GetOutputSize() const override { return lastFilter_->GetOutputSize(); }
	inline virtual std::string GetOutputFormat() const override { return lastFilter_->GetOutputFormat(); }

protected:
	std::vector<IImagePipelineComponent*> components_;

	IImageSource* lastFilter_;
};
