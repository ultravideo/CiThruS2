#pragma once

#include "RHIResources.h"
#include "Pipeline/Internal/PipelineSource.h"
#include "Pipeline/Internal/RenderTargetReaderBase.h"

#include <vector>
#include <mutex>

// Reads data from RHI render targets in VRAM and passes synchronized user data alongside each frame
class CITHRUS_API RenderTargetReaderWithUserData : public PipelineSource<2>, protected RenderTargetReaderBase
{
public:
	RenderTargetReaderWithUserData(std::vector<UTextureRenderTarget2D*> textures, const bool& depth = false, const float& depthRange = 150.0f);
	virtual ~RenderTargetReaderWithUserData();

	virtual void Process() override;

	void Read(uint8_t* userData, const uint32_t& userDataSize);

protected:
	uint8_t* userData_[2];
	uint32_t userDataSize_;

	uint8_t* queuedUserData_;
	uint32_t queuedUserDataSize_;

	virtual void AfterExtraction() override;
};
