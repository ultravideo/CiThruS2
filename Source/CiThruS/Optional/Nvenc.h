#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// NVENC includes windows.h on Windows which defines the macros min and max which break
// std::min and std::max. This prevents those macros from being defined
#ifndef NOMINMAX
#define NOMINMAX
#endif

#endif // _WIN32

#if __has_include("NVENC/Include/nvEncodeAPI.h")
#define CITHRUS_NVENC_AVAILABLE
#include "NVENC/Include/nvEncodeAPI.h"

// The current NVENC encoder block depends on DX12
#ifdef _WIN32
#include <d3d12.h>
#include <dxgi1_4.h>
#endif // _WIN32
#else
#pragma message (__FILE__ ": warning: NVIDIA Video Codec SDK not found, NVENC HEVC encoding is unavailable")
#endif // __has_include(...)

// NVENC includes windows.h which defines a macro called UpdateResource which
// breaks Unreal Engine's texture classes as they also contain a function
// called UpdateResource. The macro must be undefined for CiThruS to compile
#if defined(_WIN32) && defined(CITHRUS_NVENC_AVAILABLE)
#undef UpdateResource
#endif
