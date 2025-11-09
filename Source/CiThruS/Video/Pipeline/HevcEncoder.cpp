#include "HevcEncoder.h"
#include "Misc/Debug.h"
#include <thread>
#ifdef CITHRUS_VIDEOTOOLBOX_AVAILABLE
#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CoreVideo.h>
#include <CoreMedia/CoreMedia.h>
#include <VideoToolbox/VideoToolbox.h>
// Forward declaration for status decoding used in UE_LOG before its definition later in this file
static const TCHAR* DecodeCVStatus(OSStatus s);
#endif



const uint64_t KVAZAAR_FRAMERATE_DENOM = 90000;

HevcEncoder::HevcEncoder(const uint16_t& frameWidth, const uint16_t& frameHeight, const uint8_t& threadCount, const uint8_t& qp, const uint8_t& wpp, const uint8_t& owf, const HevcEncoderPreset& preset)
	: frameWidth_(frameWidth), frameHeight_(frameHeight), outputData_(nullptr), startTime_(std::chrono::high_resolution_clock::time_point::min())
#ifdef CITHRUS_VIDEOTOOLBOX_AVAILABLE
	, compressionSession_(nullptr), pixelBufferPool_(nullptr), frameCounter_(0), encodeComplete_(false)
#endif
{
#ifdef CITHRUS_VIDEOTOOLBOX_AVAILABLE
	// Create VideoToolbox compression session for HEVC encoding
	CFMutableDictionaryRef encoderSpec = CFDictionaryCreateMutable(
		kCFAllocatorDefault, 1,
		&kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
	
	CFDictionarySetValue(encoderSpec,
		kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder,
		kCFBooleanTrue);
	
	// Create pixel buffer attributes
	CFMutableDictionaryRef pixelBufferAttrs = CFDictionaryCreateMutable(
		kCFAllocatorDefault, 4,
		&kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
	
	CFNumberRef widthNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &frameWidth);
	CFNumberRef heightNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &frameHeight);
	// Prefer NV12 for VideoToolbox HW encoders
	OSType pixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
	CFNumberRef pixelFormatNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pixelFormat);
	// Ensure IOSurface-backed pixel buffers (required by VT on macOS/iOS)
	CFMutableDictionaryRef ioSurfProps = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
	CFDictionarySetValue(pixelBufferAttrs, kCVPixelBufferWidthKey, widthNum);
	CFDictionarySetValue(pixelBufferAttrs, kCVPixelBufferHeightKey, heightNum);
	CFDictionarySetValue(pixelBufferAttrs, kCVPixelBufferPixelFormatTypeKey, pixelFormatNum);
	CFDictionarySetValue(pixelBufferAttrs, kCVPixelBufferIOSurfacePropertiesKey, ioSurfProps);
	
	CFRelease(widthNum);
	CFRelease(heightNum);
	CFRelease(pixelFormatNum);
	CFRelease(ioSurfProps);

	UE_LOG(LogTemp, Log, TEXT("HevcEncoder: VideoToolbox support compiled in"));
	
	// Create compression session
	OSStatus status = VTCompressionSessionCreate(
		kCFAllocatorDefault,
		frameWidth,
		frameHeight,
		kCMVideoCodecType_HEVC,
		encoderSpec,
		pixelBufferAttrs,
		kCFAllocatorDefault,
		CompressionCallback,
		this,
		&compressionSession_);

	// Log session creation
	UE_LOG(LogTemp, Log, TEXT("VTCompressionSessionCreate returned status=%d, session=%p"), (int)status, compressionSession_);
	
	CFRelease(encoderSpec);
	// Don't release pixelBufferAttrs yet - keep it until after PrepareToEncodeFrames
	
	if (status == noErr && compressionSession_)
	{
		useVideoToolbox_ = true;
		// Set encoding properties based on preset
		OSStatus propStatus = noErr;
		// Low latency defaults
		propStatus = VTSessionSetProperty(compressionSession_, kVTCompressionPropertyKey_RealTime, kCFBooleanTrue);
		if (propStatus != noErr) { UE_LOG(LogTemp, Warning, TEXT("VT: Failed to set RealTime: %d"), (int)propStatus); }
		propStatus = VTSessionSetProperty(compressionSession_, kVTCompressionPropertyKey_AllowFrameReordering, kCFBooleanFalse);
		if (propStatus != noErr) { UE_LOG(LogTemp, Warning, TEXT("VT: Failed to set AllowFrameReordering: %d"), (int)propStatus); }

		// Reduce internal buffering to minimize pool pressure
		int32_t maxDelay = 1;
		CFNumberRef maxDelayNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &maxDelay);
		propStatus = VTSessionSetProperty(compressionSession_, kVTCompressionPropertyKey_MaxFrameDelayCount, maxDelayNum);
		CFRelease(maxDelayNum);
		if (propStatus != noErr) { UE_LOG(LogTemp, Warning, TEXT("VT: Failed to set MaxFrameDelayCount: %d"), (int)propStatus); }

		if (preset == HevcPresetMinimumLatency)
		{
			// Set keyframe interval - VideoToolbox seems to ignore this sometimes, so we'll also force keyframes manually
			int32_t gop = 30; // Back to 30 frames (~0.5s @60fps) - more efficient
			CFNumberRef gopNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &gop);
			propStatus = VTSessionSetProperty(compressionSession_, kVTCompressionPropertyKey_MaxKeyFrameInterval, gopNum);
			CFRelease(gopNum);
			if (propStatus != noErr) { UE_LOG(LogTemp, Warning, TEXT("VT: Failed to set MaxKeyFrameInterval: %d"), (int)propStatus); }
			
			// Also set MaxKeyFrameIntervalDuration as backup
			double gopDurationValue = 0.5; // 0.5 seconds
			CFNumberRef gopDuration = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &gopDurationValue);
			propStatus = VTSessionSetProperty(compressionSession_, kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration, gopDuration);
			CFRelease(gopDuration);
			if (propStatus != noErr) { UE_LOG(LogTemp, Warning, TEXT("VT: Failed to set MaxKeyFrameIntervalDuration: %d"), (int)propStatus); }
			
			// CRITICAL: Set a reasonable bitrate to keep frames RTP-friendly
			// Calculate: target 2 Mbps for streaming (much lower than default)
			int32_t targetBitrate = 2 * 1024 * 1024; // 2 Mbps
			CFNumberRef bitrateNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &targetBitrate);
			propStatus = VTSessionSetProperty(compressionSession_, kVTCompressionPropertyKey_AverageBitRate, bitrateNum);
			CFRelease(bitrateNum);
			if (propStatus != noErr) { UE_LOG(LogTemp, Warning, TEXT("VT: Failed to set AverageBitRate: %d"), (int)propStatus); }
			else { UE_LOG(LogTemp, Log, TEXT("VT: Set target bitrate to 2 Mbps for RTP streaming")); }
			
			// Also set data rate limits to enforce bitrate
			int32_t dataRateLimits[2] = { targetBitrate / 8, 1 }; // bytes per second, duration
			CFNumberRef dataRateNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &dataRateLimits[0]);
			CFNumberRef dataRateDuration = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &dataRateLimits[1]);
			CFArrayRef dataRateLimitsArray = CFArrayCreate(kCFAllocatorDefault, (const void*[]){dataRateNum, dataRateDuration}, 2, &kCFTypeArrayCallBacks);
			propStatus = VTSessionSetProperty(compressionSession_, kVTCompressionPropertyKey_DataRateLimits, dataRateLimitsArray);
			CFRelease(dataRateNum);
			CFRelease(dataRateDuration);
			CFRelease(dataRateLimitsArray);
			if (propStatus != noErr) { UE_LOG(LogTemp, Warning, TEXT("VT: Failed to set DataRateLimits: %d"), (int)propStatus); }
		}
		else if (preset == HevcPresetLossless)
		{
			float qualityValue = 1.0f;
			CFNumberRef qualityNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &qualityValue);
			propStatus = VTSessionSetProperty(compressionSession_, kVTCompressionPropertyKey_Quality, qualityNum);
			CFRelease(qualityNum);
			if (propStatus != noErr) { UE_LOG(LogTemp, Warning, TEXT("VT: Failed to set Quality: %d"), (int)propStatus); }
		}
		else
		{
			// Apply a rough bitrate based on QP (lower QP => higher bitrate)
			if (qp > 0)
			{
				int32_t bitrate = (51 - qp) * frameWidth * frameHeight * 2; // Rough estimate
				CFNumberRef bitrateNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &bitrate);
				propStatus = VTSessionSetProperty(compressionSession_, kVTCompressionPropertyKey_AverageBitRate, bitrateNum);
				CFRelease(bitrateNum);
				if (propStatus != noErr) { UE_LOG(LogTemp, Warning, TEXT("VT: Failed to set AverageBitRate: %d"), (int)propStatus); }
			}
		}
		
		// Set profile level
		propStatus = VTSessionSetProperty(compressionSession_, kVTCompressionPropertyKey_ProfileLevel, kVTProfileLevel_HEVC_Main_AutoLevel);
		if (propStatus != noErr) { UE_LOG(LogTemp, Warning, TEXT("VT: Failed to set ProfileLevel: %d"), (int)propStatus); }

		// Provide expected frame rate hint (improves pacing & pool sizing)
		int32_t expectedFps = 60;
		CFNumberRef fpsNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &expectedFps);
		OSStatus fpsStatus = VTSessionSetProperty(compressionSession_, kVTCompressionPropertyKey_ExpectedFrameRate, fpsNum);
		CFRelease(fpsNum);
		if (fpsStatus != noErr) { UE_LOG(LogTemp, Warning, TEXT("VT: Failed to set ExpectedFrameRate: %d"), (int)fpsStatus); }

		// Note: For HEVC, NALU length field size is dictated by format description; no property to force it.
		// Prepare the session - this should allocate the internal pixel buffer pool
		OSStatus prepStatus = VTCompressionSessionPrepareToEncodeFrames(compressionSession_);
		if (prepStatus != noErr) { 
			UE_LOG(LogTemp, Warning, TEXT("VT: PrepareToEncodeFrames failed: %d"), (int)prepStatus); 
		}

		// Now try to get the pixel buffer pool (owned by session)
		pixelBufferPool_ = VTCompressionSessionGetPixelBufferPool(compressionSession_);
		UE_LOG(LogTemp, Log, TEXT("HevcEncoder: After PrepareToEncodeFrames, PixelBufferPool=%p"), pixelBufferPool_);
		
		// Can release pixelBufferAttrs now
		CFRelease(pixelBufferAttrs);

		// If VT did not provide a pool (can happen on some OS/device combos), create our own
		if (!pixelBufferPool_)
		{
			UE_LOG(LogTemp, Warning, TEXT("HevcEncoder: VT did not provide a PixelBufferPool; creating a local pool."));
			CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			CFNumberRef wNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &frameWidth);
			CFNumberRef hNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &frameHeight);
			OSType pf = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
			CFNumberRef pfNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pf);
			CFMutableDictionaryRef ioSurfProps2 = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			CFDictionarySetValue(attrs, kCVPixelBufferWidthKey, wNum);
			CFDictionarySetValue(attrs, kCVPixelBufferHeightKey, hNum);
			CFDictionarySetValue(attrs, kCVPixelBufferPixelFormatTypeKey, pfNum);
			CFDictionarySetValue(attrs, kCVPixelBufferIOSurfacePropertiesKey, ioSurfProps2);

			CFMutableDictionaryRef poolOpts = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			int32_t minBufs = 6;
			CFNumberRef minBufsNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &minBufs);
			CFDictionarySetValue(poolOpts, kCVPixelBufferPoolMinimumBufferCountKey, minBufsNum);

			OSStatus poolStatus = CVPixelBufferPoolCreate(kCFAllocatorDefault, poolOpts, attrs, &pixelBufferPool_);
			CFRelease(minBufsNum);
			CFRelease(poolOpts);
			CFRelease(wNum); CFRelease(hNum); CFRelease(pfNum); CFRelease(ioSurfProps2); CFRelease(attrs);

			if (poolStatus == kCVReturnSuccess && pixelBufferPool_)
			{
				ownPixelBufferPool_ = true;
				// Optionally warm the pool
				CVPixelBufferRef warm[4] = {nullptr,nullptr,nullptr,nullptr};
				for (int i = 0; i < 4; ++i)
				{
					if (CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, pixelBufferPool_, &warm[i]) != kCVReturnSuccess) { warm[i] = nullptr; break; }
				}
				for (int i = 0; i < 4; ++i) { if (warm[i]) CVPixelBufferRelease(warm[i]); }
				UE_LOG(LogTemp, Log, TEXT("HevcEncoder: Created local PixelBufferPool=%p"), pixelBufferPool_);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("HevcEncoder: Failed to create local PixelBufferPool (status=%d)"), (int)poolStatus);
			}
		}
	}
#elif defined(CITHRUS_KVAZAAR_AVAILABLE)
	// Set up Kvazaar for encoding
	kvazaarConfig_ = kvazaarApi_->config_alloc();

	kvazaarApi_->config_init(kvazaarConfig_);
	kvazaarApi_->config_parse(kvazaarConfig_, "threads", std::to_string(threadCount).c_str());
	//kvazaarApi_->config_parse(kvazaarConfig_, "force-level", "4");
	//kvazaarApi_->config_parse(kvazaarConfig_, "intra_period", "16");
	//kvazaar_api->config_parse(kvazaarConfig_, "period", "64");
	//kvazaar_api->config_parse(kvazaarConfig_, "tiles", "3x3");
	//kvazaar_api->config_parse(kvazaarConfig_, "slices", "tiles");
	//kvazaar_api->config_parse(kvazaarConfig_, "mv-constraint", "frametilemargin");
	//kvazaar_api->config_parse(kvazaarConfig_, "slices", "wpp");

	switch (preset)
	{
	case HevcPresetMinimumLatency:
		kvazaarApi_->config_parse(kvazaarConfig_, "preset", "ultrafast");
		kvazaarApi_->config_parse(kvazaarConfig_, "gop", "lp-g8d1t1");
		kvazaarApi_->config_parse(kvazaarConfig_, "vps-period", "16");
		//kvazaarApi_->config_parse(kvazaarConfig_, "repeat-headers", "1"); // VPS/SPS/PPS before every keyframe
		//kvazaarApi_->config_parse(kvazaarConfig_, "vps-period", "1");

		kvazaarConfig_->qp = qp;
		kvazaarConfig_->wpp = wpp;
		kvazaarConfig_->owf = owf;
		//kvazaarConfig_->bipred = 0;
		//kvazaarConfig_->intra_period = 16;
		break;

	case HevcPresetLossless:
		kvazaarConfig_->lossless = 1;
		break;
	}

	kvazaarConfig_->width = frameWidth;
	kvazaarConfig_->height = frameHeight;
	kvazaarConfig_->hash = KVZ_HASH_NONE;
	//kvazaarConfig_->framerate_num = 30;  //1
	//kvazaarConfig_->framerate_denom = 1; //KVAZAAR_FRAMERATE_DENOM;

	kvazaarConfig_->aud_enable = 0;
	kvazaarConfig_->calc_psnr = 0;

	kvazaarEncoder_ = kvazaarApi_->encoder_open(kvazaarConfig_);
	kvazaarTransmitPicture_ = kvazaarApi_->picture_alloc(frameWidth_, frameHeight_);
#endif // CITHRUS_KVAZAAR_AVAILABLE

	GetInputPin<0>().SetAcceptedFormat("yuv420");
	GetOutputPin<0>().SetFormat("hevc");
}

HevcEncoder::~HevcEncoder()
{
#ifdef CITHRUS_VIDEOTOOLBOX_AVAILABLE
	if (compressionSession_)
	{
		VTCompressionSessionInvalidate(compressionSession_);
		CFRelease(compressionSession_);
		compressionSession_ = nullptr;
	}
	// Release local pool if we created one
	if (pixelBufferPool_ && ownPixelBufferPool_)
	{
		CVPixelBufferPoolRelease(pixelBufferPool_);
		pixelBufferPool_ = nullptr;
	}
	// pixelBufferPool_ provided by VT is owned by the session
#elif defined(CITHRUS_KVAZAAR_AVAILABLE)
	kvazaarApi_->config_destroy(kvazaarConfig_);
	kvazaarApi_->encoder_close(kvazaarEncoder_);
	kvazaarApi_->picture_free(kvazaarTransmitPicture_);

	kvazaarConfig_ = nullptr;
	kvazaarEncoder_ = nullptr;
	kvazaarTransmitPicture_ = nullptr;
#endif // CITHRUS_KVAZAAR_AVAILABLE

	delete[] outputData_;
	outputData_ = nullptr;
}

void HevcEncoder::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData || inputSize == 0)
	{
		return;
	}

	std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();

	if (startTime_ == std::chrono::high_resolution_clock::time_point::min())
	{
		startTime_ = now;
	}

	const uint8_t* yuvFrame = inputData;

#ifdef CITHRUS_VIDEOTOOLBOX_AVAILABLE
	if (useVideoToolbox_ && compressionSession_)
	{
		auto frameStartTime = std::chrono::high_resolution_clock::now();
		CVPixelBufferRef pixelBuffer = nullptr;
		
		// Always create pixel buffers manually to avoid pool allocation threshold issues
		// This is more reliable than depending on VT's internal pool behavior
		auto bufferCreateStart = std::chrono::high_resolution_clock::now();
		CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFNumberRef wNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &frameWidth_);
		CFNumberRef hNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &frameHeight_);
		OSType pf = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange; // NV12 full range
		CFNumberRef pfNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pf);
		CFMutableDictionaryRef ioSurfProps = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(attrs, kCVPixelBufferWidthKey, wNum);
		CFDictionarySetValue(attrs, kCVPixelBufferHeightKey, hNum);
		CFDictionarySetValue(attrs, kCVPixelBufferPixelFormatTypeKey, pfNum);
		CFDictionarySetValue(attrs, kCVPixelBufferIOSurfacePropertiesKey, ioSurfProps);

		OSStatus status = CVPixelBufferCreate(kCFAllocatorDefault, frameWidth_, frameHeight_, pf, attrs, &pixelBuffer);
		CFRelease(wNum); CFRelease(hNum); CFRelease(pfNum); CFRelease(ioSurfProps); CFRelease(attrs);
		
		auto bufferCreateEnd = std::chrono::high_resolution_clock::now();
		auto bufferCreateMs = std::chrono::duration<double, std::milli>(bufferCreateEnd - bufferCreateStart).count();
		
		if (status != kCVReturnSuccess || !pixelBuffer)
		{
			UE_LOG(LogTemp, Error, TEXT("HevcEncoder: CVPixelBufferCreate failed status=%d (%s)"), (int)status, DecodeCVStatus(status));
			GetOutputPin<0>().SetData(nullptr); GetOutputPin<0>().SetSize(0);
			return;
		}

		// Convert YUV data (I420) to pixel buffer
		auto conversionStart = std::chrono::high_resolution_clock::now();
		if (!ConvertYUVToPixelBuffer(yuvFrame, pixelBuffer))
		{
			UE_LOG(LogTemp, Error, TEXT("HevcEncoder: ConvertYUVToPixelBuffer failed"));
			CVPixelBufferRelease(pixelBuffer);
			GetOutputPin<0>().SetData(nullptr); GetOutputPin<0>().SetSize(0);
			return;
		}
		auto conversionEnd = std::chrono::high_resolution_clock::now();
		auto conversionMs = std::chrono::duration<double, std::milli>(conversionEnd - conversionStart).count();
		
		// Prepare presentation timestamp
		CMTime presentationTime = CMTimeMake(frameCounter_, 60); // 60 fps timebase
		CMTime duration = CMTimeMake(1, 60);
		
		// Force a keyframe every 30 frames (VideoToolbox sometimes ignores MaxKeyFrameInterval)
		// Also force first frame to be a keyframe
		CFDictionaryRef frameProperties = NULL;
		bool forceKeyframe = (frameCounter_ == 0 || frameCounter_ % 30 == 0);
		if (forceKeyframe)
		{
			CFStringRef keys[] = { kVTEncodeFrameOptionKey_ForceKeyFrame };
			CFBooleanRef values[] = { kCFBooleanTrue };
			frameProperties = CFDictionaryCreate(kCFAllocatorDefault, 
				(const void**)keys, (const void**)values, 1,
				&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			UE_LOG(LogTemp, Log, TEXT("HevcEncoder: Forcing keyframe at frame %lld"), frameCounter_);
		}
		
		frameCounter_++;
		
		// Reset synchronization
		{
			std::lock_guard<std::mutex> lock(encodeMutex_);
			encodeComplete_ = false;
			encodedData_.clear();
		}
		
		// Encode the frame
		auto encodeStart = std::chrono::high_resolution_clock::now();
		OSStatus encodeStatus = VTCompressionSessionEncodeFrame(
			compressionSession_,
			pixelBuffer,
			presentationTime,
			duration,
			frameProperties,
			NULL,
			NULL);
		
		if (frameProperties)
		{
			CFRelease(frameProperties);
		}
		
		CVPixelBufferRelease(pixelBuffer);
		
		if (encodeStatus != noErr)
		{
			UE_LOG(LogTemp, Error, TEXT("VTCompressionSessionEncodeFrame failed: %d"), (int)encodeStatus);
			GetOutputPin<0>().SetData(nullptr);
			GetOutputPin<0>().SetSize(0);
			return;
		}
		
		// Wait for encoding to complete (synchronous behavior)
		{
			std::unique_lock<std::mutex> lock(encodeMutex_);
			encodeCv_.wait(lock, [this] { return encodeComplete_; });
		}
		
		auto encodeEnd = std::chrono::high_resolution_clock::now();
		auto encodeMs = std::chrono::duration<double, std::milli>(encodeEnd - encodeStart).count();
		
		auto frameEndTime = std::chrono::high_resolution_clock::now();
		auto totalFrameMs = std::chrono::duration<double, std::milli>(frameEndTime - frameStartTime).count();
		
		// Copy encoded data to output buffer
		delete[] outputData_;
		
		if (encodedData_.empty())
		{
			outputData_ = nullptr;
			GetOutputPin<0>().SetData(nullptr);
			GetOutputPin<0>().SetSize(0);
			UE_LOG(LogTemp, Warning, TEXT("HevcEncoder: Encoded frame empty"));
		}
		else
		{
			outputData_ = new uint8_t[encodedData_.size()];
			memcpy(outputData_, encodedData_.data(), encodedData_.size());
			GetOutputPin<0>().SetData(outputData_);
			GetOutputPin<0>().SetSize(static_cast<uint32_t>(encodedData_.size()));
			
			// Log performance metrics every 30 frames to track encoding speed
			static uint32_t perfFrameCount = 0;
			static double totalBufCreate = 0, totalConversion = 0, totalEncode = 0, totalFrame = 0;
			totalBufCreate += bufferCreateMs;
			totalConversion += conversionMs;
			totalEncode += encodeMs;
			totalFrame += totalFrameMs;
			perfFrameCount++;
			
			if (perfFrameCount % 30 == 0)
			{
				double avgBufCreate = totalBufCreate / 30.0;
				double avgConversion = totalConversion / 30.0;
				double avgEncode = totalEncode / 30.0;
				double avgTotal = totalFrame / 30.0;
				double fps = 1000.0 / avgTotal;
				
				UE_LOG(LogTemp, Log, TEXT("HevcEncoder PERF (30 frames avg): BufCreate=%.2fms, YUVConv=%.2fms, Encode=%.2fms, Total=%.2fms (%.1f fps, %d bytes)"), 
					avgBufCreate, avgConversion, avgEncode, avgTotal, fps, (int)encodedData_.size());
				
				// Reset accumulators
				totalBufCreate = totalConversion = totalEncode = totalFrame = 0;
			}
		}
	}
#elif defined(CITHRUS_KVAZAAR_AVAILABLE)
	{
		memcpy(kvazaarTransmitPicture_->y, yuvFrame, frameWidth_ * frameHeight_);
		yuvFrame += frameWidth_ * frameHeight_;
		memcpy(kvazaarTransmitPicture_->u, yuvFrame, frameWidth_ * frameHeight_ / 4);
		yuvFrame += frameWidth_ * frameHeight_ / 4;
		memcpy(kvazaarTransmitPicture_->v, yuvFrame, frameWidth_ * frameHeight_ / 4);

		// TODO: Something about this doesn't work, causes choppy video. Probably dts since it affects the decoding
		/*kvazaarTransmitPicture_->pts = ((now - startTime_).count() * KVAZAAR_FRAMERATE_DENOM) / 1000000000ll;
		kvazaarTransmitPicture_->dts = kvazaarTransmitPicture_->pts;*/

		kvz_frame_info frame_info;
		kvz_data_chunk* data_out = nullptr;
		uint32_t len_out = 0;

		kvazaarApi_->encoder_encode(kvazaarEncoder_, kvazaarTransmitPicture_,
			&data_out, &len_out,
			nullptr, nullptr,
			&frame_info);

		delete[] outputData_;

		if (!data_out)
		{
			outputData_ = nullptr;

			GetOutputPin<0>().SetData(outputData_);
			GetOutputPin<0>().SetSize(0);

			return;
		}

		outputData_ = new uint8_t[len_out];
		uint8_t* data_ptr = outputData_;

		for (kvz_data_chunk* chunk = data_out; chunk != nullptr; chunk = chunk->next)
		{
			memcpy(data_ptr, chunk->data, chunk->len);
			data_ptr += chunk->len;
		}

		kvazaarApi_->chunk_free(data_out);

		kvazaarApi_->picture_free(kvazaarTransmitPicture_);
		kvazaarTransmitPicture_ = kvazaarApi_->picture_alloc(frameWidth_, frameHeight_);

		GetOutputPin<0>().SetData(outputData_);
		GetOutputPin<0>().SetSize(len_out);
	}
#else
	{
		GetOutputPin<0>().SetData(nullptr);
		GetOutputPin<0>().SetSize(0);
	}
#endif // CITHRUS_KVAZAAR_AVAILABLE
}

#ifdef CITHRUS_VIDEOTOOLBOX_AVAILABLE

// ============================================================================
// HEVC NAL Unit Inspection Helpers
// ============================================================================

// Detect if buffer starts with Annex-B start code (0x000001 or 0x00000001)
static bool StartsWithAnnexB(const uint8_t* p, size_t n)
{
	if (n >= 4 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x01)
		return true;
	if (n >= 3 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01)
		return true;
	return false;
}

// Find next Annex-B start code in buffer, return offset or -1 if not found
static int FindNextStartCode(const uint8_t* p, int start, int end)
{
	for (int i = start; i < end - 2; ++i)
	{
		if (p[i] == 0x00 && p[i+1] == 0x00)
		{
			if (p[i+2] == 0x01)
				return i; // 3-byte start code
			if (i < end - 3 && p[i+2] == 0x00 && p[i+3] == 0x01)
				return i; // 4-byte start code
		}
	}
	return -1;
}

// Extract HEVC NAL unit type from NAL header (bits 1-6 of first byte)
static uint8_t HevcNalType(const uint8_t* nalu, size_t nalu_len)
{
	if (nalu_len < 1) return 0xFF;
	return (nalu[0] & 0x7E) >> 1;
}

// Get human-readable name for HEVC NAL unit type
static FString NalTypeName(uint8_t t)
{
	switch (t)
	{
	case 0: return TEXT("TRAIL_N");
	case 1: return TEXT("TRAIL_R");
	case 2: return TEXT("TSA_N");
	case 3: return TEXT("TSA_R");
	case 4: return TEXT("STSA_N");
	case 5: return TEXT("STSA_R");
	case 6: return TEXT("RADL_N");
	case 7: return TEXT("RADL_R");
	case 8: return TEXT("RASL_N");
	case 9: return TEXT("RASL_R");
	case 16: return TEXT("BLA_W_LP");
	case 17: return TEXT("BLA_W_RADL");
	case 18: return TEXT("BLA_N_LP");
	case 19: return TEXT("IDR_W_RADL");
	case 20: return TEXT("IDR_N_LP");
	case 21: return TEXT("CRA_NUT");
	case 32: return TEXT("VPS");
	case 33: return TEXT("SPS");
	case 34: return TEXT("PPS");
	case 35: return TEXT("AUD");
	case 36: return TEXT("EOS_NUT");
	case 37: return TEXT("EOB_NUT");
	case 38: return TEXT("FD_NUT");
	case 39: return TEXT("PREFIX_SEI");
	case 40: return TEXT("SUFFIX_SEI");
	default: return FString::Printf(TEXT("TYPE_%d"), t);
	}
}

// Parse and log Annex-B access unit details
static void LogAnnexBAU(const uint8_t* data, size_t size)
{
	if (!data || size == 0) return;
	
	int nalCount = 0;
	bool hasVPS = false, hasSPS = false, hasPPS = false, hasIDR = false;
	int offset = 0;
	
	while (offset < (int)size)
	{
		// Find start code
		int scLen = 0;
		if (offset + 3 <= (int)size && data[offset] == 0x00 && data[offset+1] == 0x00 && data[offset+2] == 0x01)
			scLen = 3;
		else if (offset + 4 <= (int)size && data[offset] == 0x00 && data[offset+1] == 0x00 && data[offset+2] == 0x00 && data[offset+3] == 0x01)
			scLen = 4;
		else
			break; // No valid start code
		
		int nalStart = offset + scLen;
		int nextSc = FindNextStartCode(data, nalStart, (int)size);
		int nalEnd = (nextSc >= 0) ? nextSc : (int)size;
		int nalSize = nalEnd - nalStart;
		
		if (nalSize > 0)
		{
			uint8_t nalType = HevcNalType(data + nalStart, nalSize);
			FString nalName = NalTypeName(nalType);
			
			if (nalType == 32) hasVPS = true;
			else if (nalType == 33) hasSPS = true;
			else if (nalType == 34) hasPPS = true;
			else if (nalType == 19 || nalType == 20) hasIDR = true;
			
			UE_LOG(LogTemp, Log, TEXT("  NAL #%d: type=%d (%s), size=%d"),
				nalCount, nalType, *nalName, nalSize);
			nalCount++;
		}
		
		offset = nalEnd;
	}
	
	UE_LOG(LogTemp, Log, TEXT("HevcEncoder: AU size=%d, keyframe=%d (VPS=%d SPS=%d PPS=%d IDR=%d), NALs=%d"),
		(int)size, hasIDR ? 1 : 0, hasVPS ? 1 : 0, hasSPS ? 1 : 0, hasPPS ? 1 : 0, hasIDR ? 1 : 0, nalCount);
}

// Parse and log length-prefixed (HVCC) access unit details
static void LogHvccAU(const uint8_t* data, size_t size, int naluLengthSize)
{
	if (!data || size == 0) return;
	
	int nalCount = 0;
	bool hasVPS = false, hasSPS = false, hasPPS = false, hasIDR = false;
	size_t offset = 0;
	
	while (offset + naluLengthSize <= size)
	{
		uint32_t naluLength = 0;
		if (naluLengthSize == 4)
		{
			naluLength = (data[offset] << 24) | (data[offset+1] << 16) |
						 (data[offset+2] << 8) | data[offset+3];
		}
		else if (naluLengthSize == 2)
		{
			naluLength = (data[offset] << 8) | data[offset+1];
		}
		else if (naluLengthSize == 1)
		{
			naluLength = data[offset];
		}
		
		offset += naluLengthSize;
		
		if (naluLength == 0 || offset + naluLength > size)
			break;
		
		uint8_t nalType = HevcNalType(data + offset, naluLength);
		FString nalName = NalTypeName(nalType);
		
		if (nalType == 32) hasVPS = true;
		else if (nalType == 33) hasSPS = true;
		else if (nalType == 34) hasPPS = true;
		else if (nalType == 19 || nalType == 20) hasIDR = true;
		
		UE_LOG(LogTemp, Log, TEXT("  NAL #%d: type=%d (%s), size=%d"),
			nalCount, nalType, *nalName, (int)naluLength);
		nalCount++;
		
		offset += naluLength;
	}
	
	UE_LOG(LogTemp, Log, TEXT("HevcEncoder: AU size=%d (HVCC length-prefixed, %d-byte), keyframe=%d (VPS=%d SPS=%d PPS=%d IDR=%d), NALs=%d"),
		(int)size, naluLengthSize, hasIDR ? 1 : 0, hasVPS ? 1 : 0, hasSPS ? 1 : 0, hasPPS ? 1 : 0, hasIDR ? 1 : 0, nalCount);
}

void HevcEncoder::CompressionCallback(void* outputCallbackRefCon,
	void* sourceFrameRefCon,
	OSStatus status,
	VTEncodeInfoFlags infoFlags,
	CMSampleBufferRef sampleBuffer)
{
	HevcEncoder* encoder = static_cast<HevcEncoder*>(outputCallbackRefCon);
	UE_LOG(LogTemp, Verbose, TEXT("CompressionCallback: status=%d, infoFlags=%u, sampleBuffer=%p"), (int)status, (unsigned)infoFlags, sampleBuffer);
	if (encoder)
	{
		encoder->HandleEncodedFrame(status, sampleBuffer);
	}
}

void HevcEncoder::HandleEncodedFrame(OSStatus status, CMSampleBufferRef sampleBuffer)
{
	UE_LOG(LogTemp, Verbose, TEXT("HandleEncodedFrame: status=%d, sampleBuffer=%p"), (int)status, sampleBuffer);
	std::lock_guard<std::mutex> lock(encodeMutex_);
	
	encodedData_.clear();
	
	if (status == noErr && sampleBuffer)
	{
		// Get the encoded data from the sample buffer
		CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
		
		if (blockBuffer)
		{
			// Determine the NALU length field size from the format description
			CMFormatDescriptionRef formatDesc = CMSampleBufferGetFormatDescription(sampleBuffer);
			size_t parameterSetCount = 0;
			int nalu_header_length = 4; // default to 4 if API fails
			(void)CMVideoFormatDescriptionGetHEVCParameterSetAtIndex(formatDesc, 0, nullptr, nullptr, &parameterSetCount, &nalu_header_length);
			if (nalu_header_length != 1 && nalu_header_length != 2 && nalu_header_length != 4) {
				// guard against unexpected values
				nalu_header_length = 4;
			}
			
			// Determine if this is a keyframe
			bool isKeyframe = false;
			CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
			if (attachments && CFArrayGetCount(attachments) > 0)
			{
				CFDictionaryRef attachment = (CFDictionaryRef)CFArrayGetValueAtIndex(attachments, 0);
				CFBooleanRef dependsOnOthers = (CFBooleanRef)CFDictionaryGetValue(attachment, kCMSampleAttachmentKey_DependsOnOthers);
				isKeyframe = (dependsOnOthers == kCFBooleanFalse);
			}
			
			// APPROACH 3 WORKAROUND: Always inject parameter sets on every frame
			// This is more aggressive than standard practice but works around GStreamer rtph265depay
			// issues with out-of-band parameter sets. Trade-off: ~71 bytes overhead per frame.
			if (true) // Was: if (isKeyframe)
			{
				for (size_t i = 0; i < parameterSetCount; i++)
				{
					const uint8_t* parameterSet = nullptr;
					size_t parameterSetSize = 0;
					if (CMVideoFormatDescriptionGetHEVCParameterSetAtIndex(formatDesc, i, &parameterSet, &parameterSetSize, nullptr, nullptr) == noErr)
					{
						uint8_t nalType = HevcNalType(parameterSet, parameterSetSize);
						UE_LOG(LogTemp, Log, TEXT("HevcEncoder: Injecting parameter set NAL type=%d (%s), size=%d"),
							nalType, *NalTypeName(nalType), (int)parameterSetSize);
						encodedData_.insert(encodedData_.end(), {0x00,0x00,0x00,0x01});
						encodedData_.insert(encodedData_.end(), parameterSet, parameterSet + parameterSetSize);
					}
				}
			}
			
			// Convert AVCC-like length-prefixed stream to Annex-B format
			char* dataPtr = nullptr;
			size_t dataSize = 0;
			CMBlockBufferGetDataPointer(blockBuffer, 0, nullptr, &dataSize, &dataPtr);
			
			if (dataPtr && dataSize > 0)
			{
				size_t offset = 0;
				while (offset + nalu_header_length <= dataSize)
				{
					uint32_t naluLength = 0;
					if (nalu_header_length == 4)
					{
						naluLength = (static_cast<uint8_t>(dataPtr[offset]) << 24) |
									 (static_cast<uint8_t>(dataPtr[offset + 1]) << 16) |
									 (static_cast<uint8_t>(dataPtr[offset + 2]) << 8) |
									 static_cast<uint8_t>(dataPtr[offset + 3]);
					}
					else if (nalu_header_length == 2)
					{
						naluLength = (static_cast<uint8_t>(dataPtr[offset]) << 8) |
									 static_cast<uint8_t>(dataPtr[offset + 1]);
					}
					else // nalu_header_length == 1
					{
						naluLength = static_cast<uint8_t>(dataPtr[offset]);
					}
					offset += nalu_header_length;
					if (naluLength == 0 || offset + naluLength > dataSize)
					{
						break;
					}
					// Annex-B start code + NALU bytes
					encodedData_.insert(encodedData_.end(), {0x00,0x00,0x00,0x01});
					encodedData_.insert(encodedData_.end(), reinterpret_cast<uint8_t*>(dataPtr + offset), reinterpret_cast<uint8_t*>(dataPtr + offset + naluLength));
					offset += naluLength;
				}
			}
		}
		
		// Log the final Annex-B output with detailed NAL analysis
		if (!encodedData_.empty())
		{
			// Log first 8 bytes in hex for comparison with RTP transmitter
			if (encodedData_.size() >= 8)
			{
				UE_LOG(LogTemp, Log, TEXT("HevcEncoder: Output %d bytes, head=%02X %02X %02X %02X %02X %02X %02X %02X"),
					(int)encodedData_.size(),
					encodedData_[0], encodedData_[1], encodedData_[2], encodedData_[3],
					encodedData_[4], encodedData_[5], encodedData_[6], encodedData_[7]);
			}
			// Parse and log the Annex-B access unit
			LogAnnexBAU(encodedData_.data(), encodedData_.size());
			
			// TEMPORARY: Dump to file for validation (comment out after testing)
			#ifdef CITHRUS_HEVC_FILE_DUMP
			static FILE* dumpFile = nullptr;
			if (!dumpFile)
			{
				dumpFile = fopen("/tmp/hevc_encoder_output.265", "wb");
				if (dumpFile)
				{
					UE_LOG(LogTemp, Warning, TEXT("HevcEncoder: Dumping output to /tmp/hevc_encoder_output.265"));
				}
			}
			if (dumpFile)
			{
				fwrite(encodedData_.data(), 1, encodedData_.size(), dumpFile);
				fflush(dumpFile);
			}
			#endif
		}
	}
	
	encodeComplete_ = true;
	encodeCv_.notify_one();
}

bool HevcEncoder::ConvertYUVToPixelBuffer(const uint8_t* yuvData, CVPixelBufferRef pixelBuffer)
{
	OSStatus status = CVPixelBufferLockBaseAddress(pixelBuffer, 0);
	
	if (status != kCVReturnSuccess)
	{
		return false;
	}
	
	// Detect pixel format
	OSType fmt = CVPixelBufferGetPixelFormatType(pixelBuffer);
	// Input yuvData is I420: Y plane W*H, then U plane (W/2 * H/2), then V plane (W/2 * H/2)
	const uint8_t* srcY = yuvData;
	const uint8_t* srcU = srcY + (frameWidth_ * frameHeight_);
	const uint8_t* srcV = srcU + (frameWidth_ * frameHeight_ / 4);

	bool ok = true;
	if (fmt == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange || fmt == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)
	{
		// NV12: plane 0 is Y, plane 1 is interleaved UV
		uint8_t* yPlane = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));
		uint8_t* uvPlane = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1));
		size_t yStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
		size_t uvStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);

		// Copy Y
		for (uint32_t row = 0; row < frameHeight_; ++row)
		{
			memcpy(yPlane + row * yStride, srcY + row * frameWidth_, frameWidth_);
		}
		// Interleave U and V into UV plane
		for (uint32_t row = 0; row < frameHeight_ / 2; ++row)
		{
			uint8_t* dst = uvPlane + row * uvStride;
			const uint8_t* uRow = srcU + row * (frameWidth_ / 2);
			const uint8_t* vRow = srcV + row * (frameWidth_ / 2);
			for (uint32_t col = 0; col < frameWidth_ / 2; ++col)
			{
				// NV12 expects UV ordering (Cb then Cr)
				dst[2 * col + 0] = uRow[col];
				dst[2 * col + 1] = vRow[col];
			}
		}
	}
	else if (fmt == kCVPixelFormatType_420YpCbCr8Planar)
	{
		// I420 planar (3 planes)
		uint8_t* yPlane = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));
		uint8_t* uPlane = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1));
		uint8_t* vPlane = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 2));
		size_t yStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
		size_t uStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);
		size_t vStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 2);

		for (uint32_t row = 0; row < frameHeight_; ++row)
		{
			memcpy(yPlane + (row * yStride), srcY + (row * frameWidth_), frameWidth_);
		}
		for (uint32_t row = 0; row < frameHeight_ / 2; ++row)
		{
			memcpy(uPlane + (row * uStride), srcU + (row * frameWidth_ / 2), frameWidth_ / 2);
		}
		for (uint32_t row = 0; row < frameHeight_ / 2; ++row)
		{
			memcpy(vPlane + (row * vStride), srcV + (row * frameWidth_ / 2), frameWidth_ / 2);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("HevcEncoder: Unsupported pixel buffer format: 0x%x"), (unsigned)fmt);
		ok = false;
	}
	
	CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
	
	return ok;
}
#endif // CITHRUS_VIDEOTOOLBOX_AVAILABLE

#ifdef CITHRUS_VIDEOTOOLBOX_AVAILABLE
static const TCHAR* DecodeCVStatus(OSStatus s)
{
	switch (s)
	{
	case kCVReturnSuccess: return TEXT("kCVReturnSuccess");
	case kCVReturnInvalidArgument: return TEXT("kCVReturnInvalidArgument");
	case kCVReturnAllocationFailed: return TEXT("kCVReturnAllocationFailed");
	case kCVReturnUnsupported: return TEXT("kCVReturnUnsupported");
	case kCVReturnInvalidPixelFormat: return TEXT("kCVReturnInvalidPixelFormat");
	case kCVReturnInvalidSize: return TEXT("kCVReturnInvalidSize");
	case kCVReturnInvalidPixelBufferAttributes: return TEXT("kCVReturnInvalidPixelBufferAttributes");
	case kCVReturnPixelBufferNotOpenGLCompatible: return TEXT("kCVReturnPixelBufferNotOpenGLCompatible");
	case kCVReturnWouldExceedAllocationThreshold: return TEXT("kCVReturnWouldExceedAllocationThreshold (-6662)");
	case kCVReturnPoolAllocationFailed: return TEXT("kCVReturnPoolAllocationFailed (-6661)");
	default: return TEXT("Unknown CV status");
	}
}
#endif
