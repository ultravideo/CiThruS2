#pragma once

#if __has_include("fpng/Include/fpng.h")
#define CITHRUS_FPNG_AVAILABLE
#include "fpng/Include/fpng.h"
#else
#pragma message (__FILE__ ": warning: fpng not found, writing PNG files is unavailable")
#endif // __has_include(...)
