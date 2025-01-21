#pragma once

#include "RHIResources.h"
#include "IImageSource.h"

#include <vector>
#include <mutex>

// Extracts RHI render targets from VRAM to RAM
class CITHRUS_API RenderTargetExtractor : public IImageSource
{
public:
	RenderTargetExtractor(std::vector<UTextureRenderTarget2D*> textures, const uint8_t& threadCount = 16);
	~RenderTargetExtractor();

	void Extract();

	virtual void Process() override;

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return outputFormat_; }

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

	uint8_t threadCount_;

	// Used to prevent extracted texture data from being read and written at the same time
	std::mutex extractionMutex_;

	// Used to prevent resources from being deleted while they might still be in use on another thread
	std::mutex resourceMutex_;

	uint8_t bufferIndex_;

	bool initialized_;
};
