#pragma once

#include "RHIResources.h"
#include "IImageSource.h"

#include <vector>
#include <mutex>

// Reads data from RHI render targets in VRAM
class CITHRUS_API RenderTargetReader : public IImageSource
{
public:
	RenderTargetReader(std::vector<UTextureRenderTarget2D*> textures);
	~RenderTargetReader();

	virtual void Process() override;

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return outputFormat_; }

	void Read();

protected:
	std::vector<FRHITexture*> textures_;

	std::string outputFormat_;

	FTextureRHIRef concatBuffer_;
	FTextureRHIRef stagingBuffer_;

	uint8_t* outputFrame_;
	uint32_t outputSize_;

	uint8_t* frameBuffers_[2];

	uint16_t frameWidth_;
	uint16_t frameHeight_;
	uint8_t bytesPerPixel_;

	// Whether there's a new frame to pass onward or not
	bool frameDirty_;

	// Used to prevent texture data from being read and written at the same time
	std::mutex readMutex_;

	// Used to prevent resources from being deleted while they might still be in use on another thread
	std::mutex resourceMutex_;

	uint8_t bufferIndex_;

	// These are separate because theoretically this object may get destroyed before being initialized,
	// in which case the initialization needs to be cancelled
	bool initialized_;
	bool destroyed_;
};
