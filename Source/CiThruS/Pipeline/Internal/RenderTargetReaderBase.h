#pragma once

#include "RHIResources.h"

#include <vector>
#include <mutex>
#include <string>

// Reads data from RHI render targets in VRAM
class RenderTargetReaderBase
{
public:
	virtual ~RenderTargetReaderBase();

protected:
	uint8_t* frameBuffers_[2];

	uint16_t frameWidth_;
	uint16_t frameHeight_;
	uint8_t bytesPerPixel_;

	bool depth_;
	float depthRange_;

	std::string imageFormat_;
	uint8_t imageCount_;

	// Whether there's a new frame to pass onward or not
	bool frameDirty_;

	// Used to prevent texture data from being read and written at the same time
	std::mutex readMutex_;

	uint8_t bufferIndex_;

	RenderTargetReaderBase(std::vector<UTextureRenderTarget2D*> textures, const bool& depth, const float& depthRange);

	void ReadInternal();

	virtual void AfterExtraction() { }

	void Flush();
	void FlushCompleted();

private:
	std::vector<FRHITexture*> textures_;

	FTextureRHIRef concatBuffer_;
	FTextureRHIRef stagingBuffer_;
	FTextureRHIRef depthBuffer_;

	bool flushNeeded_;
	std::condition_variable flushCv_;
	std::mutex flushMutex_;

	// Used to prevent resources from being deleted while they might still be in use on another thread
	std::mutex resourceMutex_;

	// These are separate because theoretically this object may get destroyed before being initialized,
	// in which case the initialization needs to be cancelled
	bool initialized_;
	bool destroyed_;

	void ConvertDepth(FRHICommandListImmediate& RHICmdList) const;
	void ExtractStagingBuffer(FRHICommandListImmediate& RHICmdList);
	void CopyToStagingBuffer(FRHICommandListImmediate& RHICmdList);
};
