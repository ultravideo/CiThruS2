#include "NvencHevcEncoder.h"
#include "Misc/Debug.h"

#include <algorithm>
#include <string>

#ifdef _WIN32

#include "libloaderapi.h"

#define NVENC_API_CHECK(call) do { NVENCSTATUS s = (call); if (s != NV_ENC_SUCCESS) { UE_LOG(LogTemp, Error, TEXT("NvencHevcEncoder: NVENC call failed (%d) at " #call), static_cast<int>(s)); ShutdownImpl(); return false; } } while (0)

#define D3D12_CHECK(call) do { HRESULT hr = (call); if (FAILED(hr)) { UE_LOG(LogTemp, Error, TEXT("NvencHevcEncoder: D3D12 call failed (0x%08X) at " #call), static_cast<unsigned int>(hr)); ShutdownImpl(); return false; } } while (0)

#endif // _WIN32

NvencHevcEncoder::NvencHevcEncoder(const uint16_t& frameWidth, const uint16_t& frameHeight,
	const uint8_t& qp, const HevcEncoderPreset& preset, const uint32_t& frameRate)
	: frameWidth_(frameWidth)
	, frameHeight_(frameHeight)
	, preset_(preset)
	, frameRate_(frameRate)
	, qp_(qp)
	, outputData_(nullptr)
	, outputCapacity_(0)
	, frameIndex_(0)
	, initialized_(false)
#ifdef _WIN32
	, d3d12Device_(nullptr)
	, d3d12CommandQueue_(nullptr)
	, d3d12CommandAllocator_(nullptr)
	, d3d12CommandList_(nullptr)
	, d3d12InputTexture_(nullptr)
	, d3d12UploadBuffer_(nullptr)
	, d3d12OutputBuffer_(nullptr)
	, d3d12Fence_(nullptr)
	, d3d12FenceEvent_(nullptr)
	, d3d12FenceValue_(0)
	, d3d12UploadFootprint_{}
	, d3d12OutputBufferSize_(0)
#endif // _WIN32
#ifdef CITHRUS_NVENC_AVAILABLE
	, encoder_(nullptr)
	, registeredInput_(nullptr)
	, registeredOutput_(nullptr)
	, inputBufferFormat_(NV_ENC_BUFFER_FORMAT_UNDEFINED)
#endif // CITHRUS_NVENC_AVAILABLE
{
	GetInputPin<0>().Initialize(this, { "rgba", "bgra" });
	GetOutputPin<0>().Initialize(this, "hevc");

	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);
}

NvencHevcEncoder::~NvencHevcEncoder()
{
	ShutdownImpl();

	delete[] outputData_;
	outputData_ = nullptr;

	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);
}

void NvencHevcEncoder::OnInputPinsConnected()
{
	if (!InitializeImpl())
	{
		UE_LOG(LogTemp, Error, TEXT("NvencHevcEncoder: initialization failed; encoder will pass through null frames"));
		return;
	}

	initialized_ = true;
}

bool NvencHevcEncoder::InitializeImpl()
{
#ifndef _WIN32
	UE_LOG(LogTemp, Error, TEXT("NvencHevcEncoder: NVENC HEVC encoding is only supported on Windows"));
	return false;
#endif // _WIN32

#ifndef CITHRUS_NVENC_AVAILABLE
	UE_LOG(LogTemp, Error, TEXT("NvencHevcEncoder: NVIDIA Video Codec SDK headers not present at compile time"));
	return false;
#else
#ifdef _WIN32
	// This DLL is included with NVIDIA drivers, so if it doesn't load, it probably means NVIDIA drivers aren't
	// installed. Which in turn probably means the GPU isn't from NVIDIA and NVENC can't be used on this computer
	HMODULE libHandle = LoadLibraryEx(L"nvEncodeAPI64.dll", nullptr, 0);

	if (libHandle == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("NvencHevcEncoder: Failed to load nvEncodeAPI64.dll, are NVIDIA drivers installed?"));
		return false;
	}
#endif // _WIN32

	DXGI_FORMAT dxFormat = DXGI_FORMAT_UNKNOWN;

	std::string inputFormat = GetInputPin<0>().GetFormat();

	if (inputFormat == "rgba")
	{
		inputBufferFormat_ = NV_ENC_BUFFER_FORMAT_ABGR;
		dxFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	else if (inputFormat == "bgra")
	{
		inputBufferFormat_ = NV_ENC_BUFFER_FORMAT_ARGB;
		dxFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
	}
	else
	{
		// This should never happen if the input formats have been set properly in the constructor
		throw std::logic_error("Unsupported input format");
	}

	IDXGIFactory4* dxgiFactory = nullptr;
	D3D12_CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)));

	IDXGIAdapter1* adapter = nullptr;
	HRESULT adapterHr = dxgiFactory->EnumAdapters1(0, &adapter);
	if (FAILED(adapterHr) || adapter == nullptr)
	{
		dxgiFactory->Release();
		UE_LOG(LogTemp, Error, TEXT("NvencHevcEncoder: no DXGI adapter found"));
		ShutdownImpl();
		return false;
	}

	HRESULT deviceHr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device_));
	adapter->Release();
	dxgiFactory->Release();

	if (FAILED(deviceHr) || d3d12Device_ == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("NvencHevcEncoder: D3D12CreateDevice failed (hr=0x%08X)"), static_cast<unsigned int>(deviceHr));
		ShutdownImpl();
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask = 0;
	D3D12_CHECK(d3d12Device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3d12CommandQueue_)));

	D3D12_CHECK(d3d12Device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12Fence_)));
	d3d12FenceEvent_ = CreateEvent(nullptr, 0 , 0 , nullptr);
	if (d3d12FenceEvent_ == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("NvencHevcEncoder: CreateEvent failed"));
		ShutdownImpl();
		return false;
	}

	D3D12_HEAP_PROPERTIES defaultHeap = {};
	defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
	defaultHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	defaultHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Alignment = 0;
	textureDesc.Width = frameWidth_;
	textureDesc.Height = frameHeight_;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.MipLevels = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Format = dxFormat;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_CHECK(d3d12Device_->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE,
		&textureDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&d3d12InputTexture_)));

	uint64_t uploadTotalBytes = 0;
	d3d12Device_->GetCopyableFootprints(&textureDesc, 0, 1, 0, &d3d12UploadFootprint_, nullptr, nullptr, &uploadTotalBytes);

	D3D12_HEAP_PROPERTIES uploadHeap = {};
	uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Alignment = 0;
	bufferDesc.Width = uploadTotalBytes;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.SampleDesc.Quality = 0;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_CHECK(d3d12Device_->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE,
		&bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&d3d12UploadBuffer_)));

	D3D12_CHECK(d3d12Device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&d3d12CommandAllocator_)));
	D3D12_CHECK(d3d12Device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		d3d12CommandAllocator_, nullptr, IID_PPV_ARGS(&d3d12CommandList_)));
	D3D12_CHECK(d3d12CommandList_->Close());

	nvenc_ = { NV_ENCODE_API_FUNCTION_LIST_VER };
	NVENC_API_CHECK(NvEncodeAPICreateInstance(&nvenc_));

	NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS sessionParams = { NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER };
	sessionParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
	sessionParams.device = d3d12Device_;
	sessionParams.apiVersion = NVENCAPI_VERSION;
	NVENC_API_CHECK(nvenc_.nvEncOpenEncodeSessionEx(&sessionParams, &encoder_));

	NV_ENC_PRESET_CONFIG presetCfg = { NV_ENC_PRESET_CONFIG_VER, 0, { NV_ENC_CONFIG_VER } };
	NVENC_API_CHECK(nvenc_.nvEncGetEncodePresetConfigEx(encoder_, NV_ENC_CODEC_HEVC_GUID,
		NV_ENC_PRESET_P7_GUID, NV_ENC_TUNING_INFO_LOW_LATENCY, &presetCfg));

	NV_ENC_CONFIG encodeConfig = presetCfg.presetCfg;
	encodeConfig.profileGUID = NV_ENC_HEVC_PROFILE_MAIN_GUID;

	NV_ENC_CONFIG_HEVC& hevc = encodeConfig.encodeCodecConfig.hevcConfig;
	hevc.chromaFormatIDC = 1;
	hevc.inputBitDepth = NV_ENC_BIT_DEPTH_8;
	hevc.outputBitDepth = NV_ENC_BIT_DEPTH_8;
	hevc.outputAUD = 1;
	hevc.repeatSPSPPS = 1;
	hevc.idrPeriod = (frameRate_ > 0) ? (frameRate_ / 2) : 30;
	encodeConfig.gopLength = hevc.idrPeriod;
	encodeConfig.frameIntervalP = 1;

	NV_ENC_CONFIG_HEVC_VUI_PARAMETERS& vui = hevc.hevcVUIParameters;
	vui.videoSignalTypePresentFlag = 1;
	vui.videoFormat = NV_ENC_VUI_VIDEO_FORMAT_UNSPECIFIED;
	vui.videoFullRangeFlag = 1;
	vui.colourDescriptionPresentFlag = 1;
	vui.colourPrimaries = NV_ENC_VUI_COLOR_PRIMARIES_BT709;
	vui.transferCharacteristics = NV_ENC_VUI_TRANSFER_CHARACTERISTIC_BT709;
	vui.colourMatrix = NV_ENC_VUI_MATRIX_COEFFS_BT709;

	const uint8_t initialQP = qp_ < 1 ? 22 : qp_;
	const uint32_t cbrFps = (frameRate_ > 0) ? static_cast<uint32_t>(frameRate_) : 60u;
	encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
	encodeConfig.rcParams.averageBitRate = 600u * 1000u * 1000u;
	encodeConfig.rcParams.maxBitRate     = 600u * 1000u * 1000u;
	encodeConfig.rcParams.vbvBufferSize   = (encodeConfig.rcParams.averageBitRate / cbrFps) * 2u;
	encodeConfig.rcParams.vbvInitialDelay = encodeConfig.rcParams.vbvBufferSize;
	encodeConfig.rcParams.enableMinQP = 0;
	encodeConfig.rcParams.enableMaxQP = 1;
	encodeConfig.rcParams.maxQP.qpInterP = 14;
	encodeConfig.rcParams.maxQP.qpInterB = 14;
	encodeConfig.rcParams.maxQP.qpIntra  = 12;
	encodeConfig.rcParams.enableInitialRCQP = 1;
	encodeConfig.rcParams.initialRCQP.qpInterP = initialQP;
	encodeConfig.rcParams.initialRCQP.qpInterB = initialQP;
	encodeConfig.rcParams.initialRCQP.qpIntra  = initialQP;

	encodeConfig.rcParams.enableAQ = 1;
	encodeConfig.rcParams.aqStrength = 8;

	encodeConfig.rcParams.multiPass = NV_ENC_TWO_PASS_FULL_RESOLUTION;

	NV_ENC_INITIALIZE_PARAMS initParams = { NV_ENC_INITIALIZE_PARAMS_VER };
	initParams.encodeGUID = NV_ENC_CODEC_HEVC_GUID;
	initParams.presetGUID = NV_ENC_PRESET_P7_GUID;

	switch (preset_)
	{
	case HevcPresetMinimumLatency:
		initParams.tuningInfo = NV_ENC_TUNING_INFO_LOW_LATENCY;
		break;
	case HevcPresetLossless:
		initParams.tuningInfo = NV_ENC_TUNING_INFO_LOSSLESS;
		break;
	default:
		initParams.tuningInfo = NV_ENC_TUNING_INFO_UNDEFINED;
		break;
	}

	initParams.encodeWidth = frameWidth_;
	initParams.encodeHeight = frameHeight_;
	initParams.darWidth = frameWidth_;
	initParams.darHeight = frameHeight_;
	initParams.maxEncodeWidth = frameWidth_;
	initParams.maxEncodeHeight = frameHeight_;
	initParams.enablePTD = 1;
	initParams.enableEncodeAsync = 0;
	initParams.bufferFormat = inputBufferFormat_;
	if (frameRate_ > 0)
	{
		initParams.frameRateNum = frameRate_;
		initParams.frameRateDen = 1;
	}
	else
	{
		initParams.frameRateNum = 60;
		initParams.frameRateDen = 1;
	}
	initParams.encodeConfig = &encodeConfig;

	NVENC_API_CHECK(nvenc_.nvEncInitializeEncoder(encoder_, &initParams));

	NV_ENC_REGISTER_RESOURCE regResource = { NV_ENC_REGISTER_RESOURCE_VER };
	regResource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
	regResource.width = frameWidth_;
	regResource.height = frameHeight_;
	regResource.pitch = 0;
	regResource.subResourceIndex = 0;
	regResource.resourceToRegister = d3d12InputTexture_;
	regResource.bufferFormat = inputBufferFormat_;
	regResource.bufferUsage = NV_ENC_INPUT_IMAGE;
	NVENC_API_CHECK(nvenc_.nvEncRegisterResource(encoder_, &regResource));
	registeredInput_ = regResource.registeredResource;

	d3d12OutputBufferSize_ = (frameWidth_ * frameHeight_ * 4u) * 8u;
	d3d12OutputBufferSize_ = (d3d12OutputBufferSize_ + 3u) & ~3u;

	D3D12_HEAP_PROPERTIES readbackHeap = {};
	readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
	readbackHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	readbackHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC outputDesc = {};
	outputDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	outputDesc.Alignment = 0;
	outputDesc.Width = d3d12OutputBufferSize_;
	outputDesc.Height = 1;
	outputDesc.DepthOrArraySize = 1;
	outputDesc.MipLevels = 1;
	outputDesc.Format = DXGI_FORMAT_UNKNOWN;
	outputDesc.SampleDesc.Count = 1;
	outputDesc.SampleDesc.Quality = 0;
	outputDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	outputDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_CHECK(d3d12Device_->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE,
		&outputDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&d3d12OutputBuffer_)));

	NV_ENC_REGISTER_RESOURCE regOutput = { NV_ENC_REGISTER_RESOURCE_VER };
	regOutput.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
	regOutput.width = d3d12OutputBufferSize_;
	regOutput.height = 1;
	regOutput.pitch = 0;
	regOutput.subResourceIndex = 0;
	regOutput.resourceToRegister = d3d12OutputBuffer_;
	regOutput.bufferFormat = NV_ENC_BUFFER_FORMAT_U8;
	regOutput.bufferUsage = NV_ENC_OUTPUT_BITSTREAM;
	NVENC_API_CHECK(nvenc_.nvEncRegisterResource(encoder_, &regOutput));
	registeredOutput_ = regOutput.registeredResource;

	UE_LOG(LogTemp, Display, TEXT("NvencHevcEncoder: initialized %ux%u HEVC 4:2:0 8-bit BT.709 P7 CBR=600Mbps minQP=off maxQP=14 AQ8 twoPass=full idrEvery0.5s initialQP=%d"),
		frameWidth_, frameHeight_, static_cast<int>(initialQP));

	return true;
#endif // CITHRUS_NVENC_AVAILABLE
}

void NvencHevcEncoder::ShutdownImpl()
{
#ifdef _WIN32
#ifdef CITHRUS_NVENC_AVAILABLE
	if (encoder_ != nullptr)
	{
		if (registeredOutput_ != nullptr)
		{
			nvenc_.nvEncUnregisterResource(encoder_, registeredOutput_);
			registeredOutput_ = nullptr;
		}

		if (registeredInput_ != nullptr)
		{
			nvenc_.nvEncUnregisterResource(encoder_, registeredInput_);
			registeredInput_ = nullptr;
		}

		nvenc_.nvEncDestroyEncoder(encoder_);
		encoder_ = nullptr;
	}
#endif // CITHRUS_NVENC_AVAILABLE

	if (d3d12OutputBuffer_ != nullptr)
	{
		d3d12OutputBuffer_->Release();
		d3d12OutputBuffer_ = nullptr;
	}

	if (d3d12CommandList_ != nullptr)
	{
		d3d12CommandList_->Release();
		d3d12CommandList_ = nullptr;
	}

	if (d3d12CommandAllocator_ != nullptr)
	{
		d3d12CommandAllocator_->Release();
		d3d12CommandAllocator_ = nullptr;
	}

	if (d3d12UploadBuffer_ != nullptr)
	{
		d3d12UploadBuffer_->Release();
		d3d12UploadBuffer_ = nullptr;
	}

	if (d3d12InputTexture_ != nullptr)
	{
		d3d12InputTexture_->Release();
		d3d12InputTexture_ = nullptr;
	}

	if (d3d12Fence_ != nullptr)
	{
		d3d12Fence_->Release();
		d3d12Fence_ = nullptr;
	}

	if (d3d12FenceEvent_ != nullptr)
	{
		CloseHandle(d3d12FenceEvent_);
		d3d12FenceEvent_ = nullptr;
	}

	if (d3d12CommandQueue_ != nullptr)
	{
		d3d12CommandQueue_->Release();
		d3d12CommandQueue_ = nullptr;
	}

	if (d3d12Device_ != nullptr)
	{
		d3d12Device_->Release();
		d3d12Device_ = nullptr;
	}
#endif // _WIN32
}

void NvencHevcEncoder::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	const uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!initialized_ || inputData == nullptr || inputSize == 0)
	{
		GetOutputPin<0>().SetData(nullptr);
		GetOutputPin<0>().SetSize(0);
		return;
	}

#ifdef _WIN32
#ifdef CITHRUS_NVENC_AVAILABLE
	void* mapped = nullptr;
	D3D12_RANGE noRead = { 0, 0 };
	HRESULT mapHr = d3d12UploadBuffer_->Map(0, &noRead, &mapped);
	if (FAILED(mapHr) || mapped == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("NvencHevcEncoder: upload Map failed"));
		GetOutputPin<0>().SetData(nullptr);
		GetOutputPin<0>().SetSize(0);
		return;
	}

	const uint32_t srcRowBytes = frameWidth_ * 4u;
	const uint32_t dstRowPitch = d3d12UploadFootprint_.Footprint.RowPitch;
	uint8_t* dstRow = static_cast<uint8_t*>(mapped) + d3d12UploadFootprint_.Offset;
	const uint8_t* srcRow = inputData;
	if (dstRowPitch == srcRowBytes)
	{
		memcpy(dstRow, srcRow, static_cast<size_t>(srcRowBytes) * frameHeight_);
	}
	else
	{
		for (uint32_t y = 0; y < frameHeight_; ++y)
		{
			memcpy(dstRow, srcRow, srcRowBytes);
			dstRow += dstRowPitch;
			srcRow += srcRowBytes;
		}
	}

	d3d12UploadBuffer_->Unmap(0, nullptr);

	d3d12CommandAllocator_->Reset();
	d3d12CommandList_->Reset(d3d12CommandAllocator_, nullptr);

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = d3d12InputTexture_;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.Subresource = 0;
	d3d12CommandList_->ResourceBarrier(1, &barrier);

	D3D12_TEXTURE_COPY_LOCATION copyDst = {};
	copyDst.pResource = d3d12InputTexture_;
	copyDst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	copyDst.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION copySrc = {};
	copySrc.pResource = d3d12UploadBuffer_;
	copySrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	copySrc.PlacedFootprint = d3d12UploadFootprint_;

	d3d12CommandList_->CopyTextureRegion(&copyDst, 0, 0, 0, &copySrc, nullptr);

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
	d3d12CommandList_->ResourceBarrier(1, &barrier);

	d3d12CommandList_->Close();
	ID3D12CommandList* lists[] = { d3d12CommandList_ };
	d3d12CommandQueue_->ExecuteCommandLists(1, lists);

	const uint64_t inputFenceValue = ++d3d12FenceValue_;
	d3d12CommandQueue_->Signal(d3d12Fence_, inputFenceValue);

	NV_ENC_MAP_INPUT_RESOURCE mapInput = { NV_ENC_MAP_INPUT_RESOURCE_VER };
	mapInput.registeredResource = registeredInput_;
	NVENCSTATUS mapStatus = nvenc_.nvEncMapInputResource(encoder_, &mapInput);
	if (mapStatus != NV_ENC_SUCCESS)
	{
		UE_LOG(LogTemp, Error, TEXT("NvencHevcEncoder: nvEncMapInputResource failed (%d)"),
			static_cast<int>(mapStatus));
		GetOutputPin<0>().SetData(nullptr);
		GetOutputPin<0>().SetSize(0);
		return;
	}

	NV_ENC_MAP_INPUT_RESOURCE mapOutput = { NV_ENC_MAP_INPUT_RESOURCE_VER };
	mapOutput.registeredResource = registeredOutput_;
	NVENCSTATUS mapOutStatus = nvenc_.nvEncMapInputResource(encoder_, &mapOutput);
	if (mapOutStatus != NV_ENC_SUCCESS)
	{
		UE_LOG(LogTemp, Error, TEXT("NvencHevcEncoder: nvEncMapInputResource(output) failed (%d)"),
			static_cast<int>(mapOutStatus));
		nvenc_.nvEncUnmapInputResource(encoder_, mapInput.mappedResource);
		GetOutputPin<0>().SetData(nullptr);
		GetOutputPin<0>().SetSize(0);
		return;
	}

	const uint64_t outputFenceValue = ++d3d12FenceValue_;

	NV_ENC_INPUT_RESOURCE_D3D12 inputResource = {};
	inputResource.version = NV_ENC_INPUT_RESOURCE_D3D12_VER;
	inputResource.pInputBuffer = mapInput.mappedResource;
	inputResource.inputFencePoint.version = NV_ENC_FENCE_POINT_D3D12_VER;
	inputResource.inputFencePoint.pFence = d3d12Fence_;
	inputResource.inputFencePoint.waitValue = inputFenceValue;
	inputResource.inputFencePoint.bWait = 1;

	NV_ENC_OUTPUT_RESOURCE_D3D12 outputResource = {};
	outputResource.version = NV_ENC_OUTPUT_RESOURCE_D3D12_VER;
	outputResource.pOutputBuffer = mapOutput.mappedResource;
	outputResource.outputFencePoint.version = NV_ENC_FENCE_POINT_D3D12_VER;
	outputResource.outputFencePoint.pFence = d3d12Fence_;
	outputResource.outputFencePoint.signalValue = outputFenceValue;
	outputResource.outputFencePoint.bSignal = 1;

	NV_ENC_PIC_PARAMS picParams = { NV_ENC_PIC_PARAMS_VER };
	picParams.inputWidth = frameWidth_;
	picParams.inputHeight = frameHeight_;
	picParams.inputPitch = d3d12UploadFootprint_.Footprint.RowPitch;
	picParams.encodePicFlags = 0;
	picParams.frameIdx = static_cast<uint32_t>(frameIndex_);
	picParams.inputTimeStamp = static_cast<uint64_t>(frameIndex_);
	picParams.inputBuffer = reinterpret_cast<NV_ENC_INPUT_PTR>(&inputResource);
	picParams.outputBitstream = reinterpret_cast<NV_ENC_OUTPUT_PTR>(&outputResource);
	picParams.bufferFmt = inputBufferFormat_;
	picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;

	frameIndex_++;

	NVENCSTATUS encodeStatus = nvenc_.nvEncEncodePicture(encoder_, &picParams);

	auto publishEmpty = [&]() {
		nvenc_.nvEncUnmapInputResource(encoder_, mapOutput.mappedResource);
		nvenc_.nvEncUnmapInputResource(encoder_, mapInput.mappedResource);
		GetOutputPin<0>().SetData(nullptr);
		GetOutputPin<0>().SetSize(0);
	};

	if (encodeStatus == NV_ENC_ERR_NEED_MORE_INPUT)
	{
		publishEmpty();
		return;
	}

	if (encodeStatus != NV_ENC_SUCCESS)
	{
		UE_LOG(LogTemp, Error, TEXT("NvencHevcEncoder: nvEncEncodePicture failed (%d)"),
			static_cast<int>(encodeStatus));
		publishEmpty();
		return;
	}

	if (d3d12Fence_->GetCompletedValue() < outputFenceValue)
	{
		d3d12Fence_->SetEventOnCompletion(outputFenceValue, d3d12FenceEvent_);
		WaitForSingleObject(d3d12FenceEvent_, INFINITE);
	}

	NV_ENC_LOCK_BITSTREAM lockBitstream = { NV_ENC_LOCK_BITSTREAM_VER };
	lockBitstream.outputBitstream = reinterpret_cast<NV_ENC_OUTPUT_PTR>(&outputResource);
	lockBitstream.doNotWait = 0;

	NVENCSTATUS lockStatus = nvenc_.nvEncLockBitstream(encoder_, &lockBitstream);
	if (lockStatus != NV_ENC_SUCCESS)
	{
		UE_LOG(LogTemp, Error, TEXT("NvencHevcEncoder: nvEncLockBitstream failed (%d)"),
			static_cast<int>(lockStatus));
		publishEmpty();
		return;
	}

	const uint32_t encodedSize = lockBitstream.bitstreamSizeInBytes;
	if (encodedSize > outputCapacity_)
	{
		delete[] outputData_;
		outputData_ = new uint8_t[encodedSize];
		outputCapacity_ = encodedSize;
	}

	if (encodedSize > 0 && lockBitstream.bitstreamBufferPtr != nullptr)
	{
		memcpy(outputData_, lockBitstream.bitstreamBufferPtr, encodedSize);
	}

	nvenc_.nvEncUnlockBitstream(encoder_, lockBitstream.outputBitstream);
	nvenc_.nvEncUnmapInputResource(encoder_, mapOutput.mappedResource);
	nvenc_.nvEncUnmapInputResource(encoder_, mapInput.mappedResource);

	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(encodedSize);
#endif // CITHRUS_NVENC_AVAILABLE
#endif // _WIN32
}
