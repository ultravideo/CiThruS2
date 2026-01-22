#include "BlinkerSource.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>

BlinkerSource::BlinkerSource(const uint16_t& width, const uint16_t& height, const double& frequency, const std::string& logPath, const std::string& format) : bufferIndex_(1), nsBetweenSwaps_(500000000.0 / frequency)
{
	if (format == "bgra" || format == "rgba")
	{
		std::array<uint8_t, 4> blackData = { 0, 0, 0, 255 };
		std::array<uint8_t, 4> whiteData = { 255, 255, 255, 255 };

		uint32_t outputSize = width * height * 4;
		
		GetOutputPin<0>().Initialize(this, format);
		GetOutputPin<0>().SetSize(outputSize);

		frameBuffers_[0] = new uint8_t[outputSize];
		frameBuffers_[1] = new uint8_t[outputSize];

		std::fill_n(reinterpret_cast<std::array<uint8_t, 4>*>(frameBuffers_[0]), width * height, blackData);
		std::fill_n(reinterpret_cast<std::array<uint8_t, 4>*>(frameBuffers_[1]), width * height, whiteData);
	}
	else
	{
		throw std::invalid_argument("Unsupported format " + format);
	}

	GetOutputPin<0>().SetData(frameBuffers_[0]);

	if (logPath != "")
	{
		logStream_ = std::ofstream(logPath, std::ios_base::out);
	}
}

BlinkerSource::~BlinkerSource()
{
	delete[] frameBuffers_[0];
	delete[] frameBuffers_[1];

	frameBuffers_[0] = nullptr;
	frameBuffers_[1] = nullptr;

	if (logStream_.is_open())
	{
		logStream_.close();
	}
}

void BlinkerSource::Process()
{
	uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

	if ((now / nsBetweenSwaps_) % 2 == bufferIndex_)
	{
		return;
	}

	logStream_ << now << " " << (bufferIndex_ == 1 ? "white" : "black") << std::endl;

	// Alternate between black and white
	GetOutputPin<0>().SetData(frameBuffers_[bufferIndex_]);
	bufferIndex_ = (bufferIndex_ + 1) % 2;
}
