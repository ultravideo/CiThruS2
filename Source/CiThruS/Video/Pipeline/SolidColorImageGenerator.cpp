#include "SolidColorImageGenerator.h"

#include <algorithm>
#include <array>

SolidColorImageGenerator::SolidColorImageGenerator(const uint16_t& width, const uint16_t& height, const uint8_t& red, const uint8_t& green, const uint8_t& blue, const uint8_t& alpha, const std::string& format)
{
	outputSize_ = width * height * 4;
	outputFrame_ = new uint8_t[outputSize_];

	std::array<uint8_t, 4> data;

	if (format == "bgra")
	{
		data = { blue, green, red, alpha };
		outputFormat_ = "bgra";
	}
	else
	{
		data = { red, green, blue, alpha };
		outputFormat_ = "rgba";
	}

	std::fill_n(reinterpret_cast<std::array<uint8_t, 4>*>(outputFrame_), width * height, data);
}

SolidColorImageGenerator::SolidColorImageGenerator(const uint16_t& width, const uint16_t& height, const uint8_t& y, const uint8_t& u, const uint8_t& v)
{
	outputSize_ = width * height * 3 / 2;
	outputFrame_ = new uint8_t[outputSize_];
	outputFormat_ = "yuv420";

	std::fill_n(outputFrame_, width * height, y);
	std::fill_n(outputFrame_ + width * height, width * height / 4, u);
	std::fill_n(outputFrame_ + width * height * 5 / 4, width * height / 4, v);
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
