#pragma once

#include "RHIResources.h"
#include "Pipeline/Internal/PipelineSink.h"

#include <vector>
#include <mutex>

class UTexture2D;

// Writes data into RHI render targets in VRAM
class CITHRUS_API RenderTargetWriter : public PipelineSink<1>
{
public:
	RenderTargetWriter(UTextureRenderTarget2D* texture);
	~RenderTargetWriter();

	virtual void Process() override;

protected:
	uint8_t* inputBuffer_;

	FRHITexture* texture_;

	uint16_t frameWidth_;
	uint16_t frameHeight_;
	uint8_t bytesPerPixel_;

	// Whether a new frame should be passed onward or not
	bool frameDirty_;

	// Used to prevent texture data from being read and written at the same time
	std::mutex writeMutex_;

	// Used to prevent resources from being deleted while they might still be in use on another thread
	std::mutex resourceMutex_;

	// These are separate because theoretically this object may get destroyed before being initialized,
	// in which case the initialization needs to be cancelled
	bool initialized_;
	bool destroyed_;
};
