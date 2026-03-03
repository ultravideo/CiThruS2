#pragma once

#if __has_include("mqtt/client.h")
#define CITHRUS_PAHOCPP_AVAILABLE
// Unreal Engine defines a macro called verify that breaks Paho
#pragma push_macro("verify")
#undef verify

// Paho implicitly converts an integer to a bool somewhere, MSVC complains about it
#ifdef _MSC_VER
#pragma warning ( push )
#pragma warning ( disable: 4800 )
#endif // _MSC_VER

#include "mqtt/client.h"

#ifdef _MSC_VER
#pragma warning ( pop )
#endif // _MSC_VER

#pragma pop_macro("verify")
#else
#pragma message (__FILE__ ": warning: Eclipse Paho MQTT C++ client library not found, MQTT is unavailable")
#endif // __has_include(...)
