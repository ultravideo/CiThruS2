#pragma once

#include "PoseEstimationMarkerGroup.h"

#ifdef _WIN32

// winsock2.h defines the macros min and max which break std::min and std::max.
// This prevents those macros from being defined
#ifndef NOMINMAX
#define NOMINMAX
#endif

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

// winsock2.h defines a macro called UpdateResource which breaks
// Unreal Engine's texture classes as they also contain a function called
// UpdateResource. The macro must be undefined for CiThruS to compile
#undef UpdateResource

#else
#pragma message (__FILE__ ": warning: Annotation streaming not available: currently only implemented for Windows")
#endif // _WIN32

#include <list>
#include <string>

// Transmits marker data using UDP
// TODO: This class is Windows-only, not synchronized properly, not used anymore. Decide whether to update or just delete
class AnnotationTransmitter
{
public:
	AnnotationTransmitter(const std::string& destinationIp, const uint16_t& destinationPort);
	~AnnotationTransmitter();

	void Transmit(const uint32_t& annotationIndex, const std::list<MarkerCaptureData>& markerData);

protected:
#ifdef _WIN32
	SOCKET socket_;
	sockaddr_in clientAddress_;
#endif // _WIN32
};
