#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// uvgRTP includes winsock2.h on Windows which defines the macros min and max which break
// std::min and std::max. This prevents those macros from being defined
#ifndef NOMINMAX
#define NOMINMAX
#endif

#endif // _WIN32

#if __has_include("uvgRTP/Include/lib.hh")
// Pre-4.0.0 uvgRTP 
#define CITHRUS_UVGRTP_AVAILABLE
#include "uvgRTP/Include/lib.hh"
#elif __has_include("uvgrtp/lib.hh")
// uvgRTP 4.0.0 and onward
#define CITHRUS_UVGRTP_AVAILABLE
#include "uvgrtp/lib.hh"
#else
#pragma message (__FILE__ ": warning: uvgRTP not found, RTP streaming is unavailable")
#endif // __has_include(...)

// uvgRTP includes winsock2.h which defines a macro called UpdateResource which
// breaks Unreal Engine's texture classes as they also contain a function
// called UpdateResource. The macro must be undefined for CiThruS to compile
#if defined(_WIN32) && defined(CITHRUS_UVGRTP_AVAILABLE)
#undef UpdateResource
#endif

namespace Optional
{
	inline bool UvgRtpAvailable()
	{
#ifdef CITHRUS_UVGRTP_AVAILABLE
		return true;
#else
		return false;
#endif // CITHRUS_UVGRTP_AVAILABLE
	}
}
