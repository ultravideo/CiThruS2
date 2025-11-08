#pragma once

#if __has_include("Kvazaar/Include/kvazaar.h")
#ifndef CITHRUS_KVAZAAR_AVAILABLE
#define CITHRUS_KVAZAAR_AVAILABLE
#endif // CITHRUS_KVAZAAR_AVAILABLE
#include "Kvazaar/Include/kvazaar.h"
#else
#pragma message (__FILE__ ": warning: Kvazaar not found, HEVC encoding is unavailable")
#endif // __has_include(...)

#if defined(CITHRUS_VIDEOTOOLBOX_AVAILABLE)
#include <VideoToolbox/VideoToolbox.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#endif // CITHRUS_VIDEOTOOLBOX_AVAILABLE

#include "PipelineFilter.h"
#include "CoreMinimal.h"

#include <string>
#include <chrono>

enum HevcEncoderPreset : uint8_t
{
	HevcPresetNone,
	HevcPresetMinimumLatency,
	HevcPresetLossless
};

// Encodes YUV 4:2:0 data into HEVC video using Kvazaar
class CITHRUS_API HevcEncoder : public PipelineFilter<1, 1>
{
public:
	HevcEncoder(const uint16_t& frameWidth, const uint16_t& frameHeight, const uint8_t& threadCount, const uint8_t& qp, const uint8_t& wpp, const uint8_t& owf, const HevcEncoderPreset& preset = HevcPresetNone);
	virtual ~HevcEncoder();

	virtual void Process() override;

protected:
	uint32_t frameWidth_;
	uint32_t frameHeight_;

	uint8_t* outputData_;

	std::chrono::high_resolution_clock::time_point startTime_;

#ifdef CITHRUS_KVAZAAR_AVAILABLE
	const kvz_api* kvazaarApi_ = kvz_api_get(8);
	kvz_config* kvazaarConfig_;
	kvz_encoder* kvazaarEncoder_;
	kvz_picture* kvazaarTransmitPicture_;
#endif // CITHRUS_KVAZAAR_AVAILABLE

#ifdef CITHRUS_VIDEOTOOLBOX_AVAILABLE
	VTCompressionSessionRef compressionSession_;
	CVPixelBufferPoolRef pixelBufferPool_;
	bool ownPixelBufferPool_ = false; // true if we created the pool ourselves
	int64_t frameCounter_;
	
	// Synchronization for async encoding
	std::mutex encodeMutex_;
	std::condition_variable encodeCv_;
	bool encodeComplete_;
	
	// Output buffer
	std::vector<uint8_t> encodedData_;
	
	static void CompressionCallback(void* outputCallbackRefCon,
		void* sourceFrameRefCon,
		OSStatus status,
		VTEncodeInfoFlags infoFlags,
		CMSampleBufferRef sampleBuffer);
	
	void HandleEncodedFrame(OSStatus status, CMSampleBufferRef sampleBuffer);
	bool ConvertYUVToPixelBuffer(const uint8_t* yuvData, CVPixelBufferRef pixelBuffer);
	bool useVideoToolbox_ = false; // true only if VT session created successfully
#endif // CITHRUS_VIDEOTOOLBOX_AVAILABLE
};
