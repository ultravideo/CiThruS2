#pragma once

#if __has_include("Kvazaar/Include/kvazaar.h")
#define CITHRUS_KVAZAAR_AVAILABLE
#include "Kvazaar/Include/kvazaar.h"
#else
#pragma message (__FILE__ ": warning: Kvazaar not found, HEVC encoding is unavailable")
#endif // __has_include(...)
