#include "AnnotationTransmitter.h"

#include "Misc/Debug.h"

#include <vector>
#include <string>

#ifdef _WIN32
#include <ws2tcpip.h>
#endif // _WIN32

AnnotationTransmitter::AnnotationTransmitter(const std::string& destinationIp, const uint16_t& destinationPort)
{
#ifdef _WIN32
	socket_ = socket(AF_INET, SOCK_DGRAM, 0);

	sockaddr_in serverAddress = { };
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(0);
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	
	int result = bind(socket_, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress));

	if (result < 0)
	{
		return;
	}

	clientAddress_ = { };
	clientAddress_.sin_family = AF_INET;
	clientAddress_.sin_port = htons(destinationPort);

	inet_pton(AF_INET, destinationIp.c_str(), &clientAddress_.sin_addr.s_addr);
#endif // _WIN32
}

AnnotationTransmitter::~AnnotationTransmitter()
{
#ifdef _WIN32
	closesocket(socket_);
#endif // _WIN32
}

void AnnotationTransmitter::Transmit(const uint32_t& annotationIndex, const std::list<MarkerCaptureData>& markerData)
{
#ifdef _WIN32
	std::vector<char> buffer(1024, 0);

	uint32_t markerDataSize = markerData.size();

	memcpy(buffer.data(), &annotationIndex, sizeof(uint32_t));
	memcpy(buffer.data() + sizeof(uint32_t), &markerDataSize, sizeof(uint32_t));

	int i = sizeof(uint32_t) * 2;

	for (const MarkerCaptureData& marker : markerData)
	{
		memcpy(buffer.data() + i, &marker, sizeof(MarkerCaptureData));
		i += sizeof(MarkerCaptureData);
	}

	sendto(socket_, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&clientAddress_), sizeof(clientAddress_));
#endif // _WIN32
}
