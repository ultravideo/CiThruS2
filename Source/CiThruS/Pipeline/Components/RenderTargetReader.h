#pragma once

#include "RHIResources.h"
#include "Pipeline/Internal/PipelineSource.h"
#include "Pipeline/Internal/RenderTargetReaderBase.h"

#include <vector>
#include <mutex>

// Reads data from RHI render targets in VRAM
class CITHRUS_API RenderTargetReader : public PipelineSource<1>, protected RenderTargetReaderBase
{
public:
	RenderTargetReader(std::vector<UTextureRenderTarget2D*> textures, const bool& depth = false, const float& depthRange = 150.0f);
	virtual ~RenderTargetReader();

	virtual void Process() override;

	void Read();
};
