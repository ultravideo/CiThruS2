#pragma once

#if __has_include("Kvazaar/Include/kvazaar.h")
#define CITHRUS_KVAZAAR_AVAILABLE
#include "Kvazaar/Include/kvazaar.h"
#else
#pragma message (__FILE__ ": warning: Kvazaar not found, HEVC encoding is unavailable")
#endif // __has_include(...)

namespace Optional
{
	inline bool KvazaarAvailable()
	{
#ifdef CITHRUS_KVAZAAR_AVAILABLE
		return true;
#else
		return false;
#endif // CITHRUS_KVAZAAR_AVAILABLE
	}
}
