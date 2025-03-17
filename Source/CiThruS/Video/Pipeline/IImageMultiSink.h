#pragma once

#include "IImagePipelineComponent.h"

#include <cstdint>

class IImageSink;

// Interface for image pipeline components that receive multiple buffers of image data
template <uint8_t N>
class IImageMultiSink : public virtual IImagePipelineComponent
{
public:
	virtual ~IImageMultiSink() { }

	template <uint8_t I>
	inline IImageSink* GetSinkAtIndex() const
	{
		// Template functions can't be virtual, so doing it like this allows the use of
		// static asserts on the index while also allowing the actual indexing logic to be
		// overwritten
		static_assert(I < N, "Sink index out of bounds");
		return GetSinkAtIndexInternal(I);
	}

protected:
	IImageMultiSink() { }

	virtual IImageSink* GetSinkAtIndexInternal(const uint8_t& index) const = 0;
};
