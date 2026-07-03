#pragma once

#include "Optional/Nvenc.h"
#include "Pipeline/Internal/PipelineFilter.h"
#include "HevcEncoder.h"
#include "CoreMinimal.h"

#include <cstdint>

// Encodes RGBA or BGRA data into HEVC video using NVENC
class CITHRUS_API NvencHevcEncoder : public PipelineFilter<1, 1>
{
public:
	NvencHevcEncoder(const uint16_t& frameWidth, const uint16_t& frameHeight,
		const uint8_t& qp, const HevcEncoderPreset& preset = HevcPresetNone, const uint32_t& frameRate = 0);
	virtual ~NvencHevcEncoder();

	virtual void Process() override;

protected:
	uint32_t frameWidth_;
	uint32_t frameHeight_;
	HevcEncoderPreset preset_;
	uint32_t frameRate_;
	uint8_t qp_;

	uint8_t* outputData_;
	uint32_t outputCapacity_;
	int64_t frameIndex_;

	bool initialized_;

	// TODO: this should probably use CUDA instead of DX12 for cross-platform support
#ifdef _WIN32
	ID3D12Device* d3d12Device_;
	ID3D12CommandQueue* d3d12CommandQueue_;
	ID3D12CommandAllocator* d3d12CommandAllocator_;
	ID3D12GraphicsCommandList* d3d12CommandList_;
	ID3D12Resource* d3d12InputTexture_;
	ID3D12Resource* d3d12UploadBuffer_;
	ID3D12Resource* d3d12OutputBuffer_;
	ID3D12Fence* d3d12Fence_;
	HANDLE d3d12FenceEvent_;
	uint64_t d3d12FenceValue_;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT d3d12UploadFootprint_;
	uint32_t d3d12OutputBufferSize_;
#endif // _WIN32

#ifdef CITHRUS_NVENC_AVAILABLE
	NV_ENCODE_API_FUNCTION_LIST nvenc_;
	void* encoder_;
	NV_ENC_REGISTERED_PTR registeredInput_;
	NV_ENC_REGISTERED_PTR registeredOutput_;
	NV_ENC_BUFFER_FORMAT inputBufferFormat_;
#endif // CITHRUS_NVENC_AVAILABLE

	virtual void OnInputPinsConnected() override;

private:
	bool InitializeImpl();
	void ShutdownImpl();
};
