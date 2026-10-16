#pragma once

#if __has_include("fpng/Include/fpng.h")
#define CITHRUS_FPNG_AVAILABLE
#include "fpng/Include/fpng.h"
#else
#pragma message (__FILE__ ": warning: fpng not found, writing PNG files is unavailable")
#endif // __has_include(...)

namespace Optional
{
	inline bool FpngAvailable()
	{
#ifdef CITHRUS_FPNG_AVAILABLE
		return true;
#else
		return false;
#endif // CITHRUS_FPNG_AVAILABLE
	}
}
