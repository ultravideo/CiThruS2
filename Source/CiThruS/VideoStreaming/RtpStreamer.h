#pragma once

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN

// uvgRTP includes winsock2.h on Windows which defines the macros min and max which break
// std::min and std::max. This prevents those macros from being defined
#ifndef NOMINMAX
#define NOMINMAX
#endif

#endif // _WIN32

#if __cplusplus >= 201703L || _MSC_VER > 1911
#if __has_include("../ThirdParty/uvgRTP/Include/lib.hh")
#define CITHRUS_UVGRTP_AVAILABLE
#include "../ThirdParty/uvgRTP/Include/lib.hh"
#else
#pragma message (__FILE__ ": warning: uvgRTP not found, RTP streaming is unavailable")
#endif // __has_include(...)
#else
#include "../ThirdParty/uvgRTP/Include/lib.hh"
#endif // __cplusplus(...)

// uvgRTP includes winsock2.h which defines a macro called UpdateResource which
// breaks Unreal Engine's texture classes as they also contain a function
// called UpdateResource. The macro must be undefined for CiThruS to compile
#if defined(_WIN32) && defined(CITHRUS_UVGRTP_AVAILABLE)
#undef UpdateResource
#endif

#include "CoreMinimal.h"
#include "IImageSink.h"

#include <string>

// Streams HEVC video through an RTP stream
class CITHRUS_API RtpStreamer : public IImageSink
{
public:
	RtpStreamer(const std::string& ip, const int& dstPort);
	virtual ~RtpStreamer();

	virtual void Process() override;

	virtual void SetInput(const IImageSource* source) override;
	inline virtual std::string GetInputFormat() const override { return "hevc"; }

protected:
	uint8_t* const* inputFrame_;
	const uint32_t* inputSize_;

#ifdef CITHRUS_UVGRTP_AVAILABLE
	uvgrtp::context streamContext_;
	uvgrtp::session* streamSession_;
	uvgrtp::media_stream* stream_;
#endif // CITHRUS_UVGRTP_AVAILABLE
};
