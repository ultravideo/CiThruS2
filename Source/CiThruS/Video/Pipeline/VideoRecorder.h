#pragma once

#include "CoreMinimal.h"
#include "IImageSink.h"

#include <string>

class IImageSource;

// TODO this is not implemented
class CITHRUS_API VideoRecorder : public IImageSink
{
public:
	VideoRecorder();
	virtual ~VideoRecorder();

	virtual void Process() override;

	virtual bool SetInput(const IImageSource* source) override;

protected:
	uint8_t* const* inputFrame_;
	const uint32_t* inputSize_;
};
