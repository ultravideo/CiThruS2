#pragma once

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN

// uvgRTP includes winsock2.h on Windows which defines the macros min and max which break
// std::min and std::max. This prevents those macros from being defined
#ifndef NOMINMAX
#define NOMINMAX
#endif

#endif // _WIN32

#if __has_include("../ThirdParty/uvgRTP/Include/lib.hh")
#ifndef CITHRUS_UVGRTP_AVAILABLE
#define CITHRUS_UVGRTP_AVAILABLE
#endif // CITHRUS_UVGRTP_AVAILABLE
#include "../ThirdParty/uvgRTP/Include/lib.hh"
#else
#pragma message (__FILE__ ": warning: uvgRTP not found, RTP streaming is unavailable")
#endif // __has_include(...)

// uvgRTP includes winsock2.h which defines a macro called UpdateResource which
// breaks Unreal Engine's texture classes as they also contain a function
// called UpdateResource. The macro must be undefined for CiThruS to compile
#if defined(_WIN32) && defined(CITHRUS_UVGRTP_AVAILABLE)
#undef UpdateResource
#endif

#include "CoreMinimal.h"
#include "IImageSource.h"

#include <string>
#include <queue>
#include <mutex>

// Receives data from an RTP stream. Currently HEVC data only
class CITHRUS_API RtpReceiver : public IImageSource
{
public:
	RtpReceiver(const std::string& ip, const int& srcPort);
	virtual ~RtpReceiver();

	virtual void Process() override;

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return "hevc"; }

protected:
	uint8_t* outputFrame_;
	uint32_t outputSize_;

	bool destroyed_;

	std::mutex queueMutex_;

#ifdef CITHRUS_UVGRTP_AVAILABLE
	uvgrtp::context streamContext_;
	uvgrtp::session* streamSession_;
	uvgrtp::media_stream* stream_;

	uvgrtp::frame::rtp_frame* currentFrame_;
	std::queue<uvgrtp::frame::rtp_frame*> frameQueue_;

	static void ReceiveAsync(void* args, uvgrtp::frame::rtp_frame* frame);
#endif // CITHRUS_UVGRTP_AVAILABLE
};
