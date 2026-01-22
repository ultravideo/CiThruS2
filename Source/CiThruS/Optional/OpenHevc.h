#pragma once

#if __has_include("OpenHEVC/Include/openHevcWrapper.h")
#define CITHRUS_OPENHEVC_AVAILABLE
#include "OpenHEVC/Include/openHevcWrapper.h"
#else
#pragma message (__FILE__ ": warning: OpenHEVC not found, HEVC decoding is unavailable")
#endif // __has_include(...)
