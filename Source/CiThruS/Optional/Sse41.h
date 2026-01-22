#pragma once

#if (defined(__x86_64__) || defined(_M_X64) && !defined(_M_ARM64EC))
#define CITHRUS_SSE41_AVAILABLE
#else
#pragma message (__FILE__ ": warning: SSE4.1 instructions not available. Relevant optimizations are disabled")
#endif // defined(...)
