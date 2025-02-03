#include "SolidColorImageGenerator.h"

#include <algorithm>
#include <array>

SolidColorImageGenerator::SolidColorImageGenerator(const uint16_t& width, const uint16_t& height, const uint8_t& red, const uint8_t& green, const uint8_t& blue, const uint8_t& alpha)
{
	outputSize_ = width * height * 4;
	outputFrame_ = new uint8_t[outputSize_];

	std::array<uint8_t, 4> rgba = { red, green, blue, alpha };

	std::fill_n(reinterpret_cast<std::array<uint8_t, 4>*>(outputFrame_), width * height, rgba);
}

SolidColorImageGenerator::~SolidColorImageGenerator()
{
	delete outputFrame_;
	outputFrame_ = nullptr;
	outputSize_ = 0;
}

void SolidColorImageGenerator::Process()
{
	// Nothing to process, the image is constant
}