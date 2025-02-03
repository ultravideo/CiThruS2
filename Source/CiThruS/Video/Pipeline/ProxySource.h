#pragma once

#include "IImageSource.h"

// A source that doesn't generate any data and instead just forwards data from somewhere else
class ProxySource : public IImageSource
{
public:
	ProxySource(uint8_t* const** buffer, const uint32_t** bufferSize, const std::string& bufferFormat)
		: buffer_(buffer), bufferSize_(bufferSize), bufferFormat_(bufferFormat) { }
	~ProxySource() { }

	inline virtual void Process() override { }

	inline virtual uint8_t* const* GetOutput() const override { return *buffer_; }
	inline virtual const uint32_t* GetOutputSize() const override { return *bufferSize_; }
	inline virtual std::string GetOutputFormat() const override { return bufferFormat_; }

protected:
	uint8_t* const* const* buffer_;
	const uint32_t** bufferSize_;
	const std::string bufferFormat_;
};
