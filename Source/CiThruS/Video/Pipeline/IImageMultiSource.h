#pragma once

#include "IImagePipelineComponent.h"

#include <cstdint>

class IImageSource;

// Interface for image pipeline components that provide multiple buffers of image data
template <uint8_t N>
class IImageMultiSource : public virtual IImagePipelineComponent
{
public:
	virtual ~IImageMultiSource() { }

	template <uint8_t I>
	inline IImageSource* GetSourceAtIndex() const
	{
		// Template functions can't be virtual, so doing it like this allows the use of
		// static asserts on the index while also allowing the actual indexing logic to be
		// overwritten
		static_assert(I < N, "Source index out of bounds");
		return GetSourceAtIndexInternal(N);
	}

protected:
	IImageMultiSource() { }

	virtual IImageSource* GetSourceAtIndexInternal(const uint8_t& index) const = 0;
};
