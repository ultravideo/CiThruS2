#pragma once

#include "RHIResources.h"
#include "PipelineSource.h"

#include <vector>
#include <mutex>

// Reads data from RHI render targets in VRAM
class CITHRUS_API RenderTargetReader : public PipelineSource<2>
{
public:
	RenderTargetReader(std::vector<UTextureRenderTarget2D*> textures, const bool& depth = false, const float& depthRange = 150.0f);
	~RenderTargetReader();

	virtual void Process() override;

	void Read(uint8_t* userData = nullptr, const uint32_t& userDataSize = 0);

protected:
	std::vector<FRHITexture*> textures_;

	FTextureRHIRef concatBuffer_;
	FTextureRHIRef stagingBuffer_;
	FTextureRHIRef depthBuffer_;

	uint8_t* frameBuffers_[2];

	uint8_t* userData_[2];
	uint32_t userDataSize_;

	uint8_t* queuedUserData_;
	uint32_t queuedUserDataSize_;

	uint16_t frameWidth_;
	uint16_t frameHeight_;
	uint8_t bytesPerPixel_;

	bool depth_;
	float depthRange_;

	// Whether there's a new frame to pass onward or not
	bool frameDirty_;

	bool flushNeeded_;
	std::condition_variable flushCv_;
	std::mutex flushMutex_;

	// Used to prevent texture data from being read and written at the same time
	std::mutex readMutex_;

	// Used to prevent resources from being deleted while they might still be in use on another thread
	std::mutex resourceMutex_;

	uint8_t bufferIndex_;

	// These are separate because theoretically this object may get destroyed before being initialized,
	// in which case the initialization needs to be cancelled
	bool initialized_;
	bool destroyed_;

	void Flush();
	void ConvertDepth(FRHICommandListImmediate& RHICmdList) const;
	void ExtractStagingBuffer(FRHICommandListImmediate& RHICmdList);
	void CopyToStagingBuffer(FRHICommandListImmediate& RHICmdList);
};
