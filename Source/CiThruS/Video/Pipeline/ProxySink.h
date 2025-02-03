#pragma once

#include "IImageSink.h"

// A sink that doesn't do anything with its received data and just passes it somewhere else
class ProxySink : public IImageSink
{
public:
	ProxySink(uint8_t* const** buffer, const uint32_t** bufferSize, const std::string& bufferFormat)
		: buffer_(buffer), bufferSize_(bufferSize), bufferFormat_(bufferFormat) { }
	~ProxySink() { }

	inline virtual void Process() override { }

	inline virtual bool SetInput(const IImageSource* source) override
	{
		if (source->GetOutputFormat() != bufferFormat_)
		{
			return false;
		}
		
		*buffer_ = source->GetOutput();

		if (bufferSize_)
		{
			*bufferSize_ = source->GetOutputSize();
		}

		return true;
	}

protected:
	uint8_t* const** buffer_;
	const uint32_t** bufferSize_;
	const std::string bufferFormat_;
};
