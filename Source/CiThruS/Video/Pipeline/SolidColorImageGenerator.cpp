#include "SolidColorImageGenerator.h"

#include <algorithm>
#include <array>

SolidColorImageGenerator::SolidColorImageGenerator(const uint16_t& width, const uint16_t& height, const uint8_t& red, const uint8_t& green, const uint8_t& blue, const uint8_t& alpha, const std::string& format)
{
	uint32_t outputSize = width * height * 4;
	outputData_ = new uint8_t[outputSize];

	std::array<uint8_t, 4> data;

	if (format == "bgra")
	{
		data = { blue, green, red, alpha };
		GetOutputPin<0>().SetFormat("bgra");
	}
	else
	{
		data = { red, green, blue, alpha };
		GetOutputPin<0>().SetFormat("rgba");
	}

	std::fill_n(reinterpret_cast<std::array<uint8_t, 4>*>(outputData_), width * height, data);

	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(outputSize);
}

SolidColorImageGenerator::SolidColorImageGenerator(const uint16_t& width, const uint16_t& height, const uint8_t& y, const uint8_t& u, const uint8_t& v)
{
	uint32_t outputSize = width * height * 3 / 2;
	outputData_ = new uint8_t[outputSize];

	std::fill_n(outputData_, width * height, y);
	std::fill_n(outputData_ + width * height, width * height / 4, u);
	std::fill_n(outputData_ + width * height * 5 / 4, width * height / 4, v);

	GetOutputPin<0>().SetFormat("yuv420");
	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(outputSize);
}

SolidColorImageGenerator::~SolidColorImageGenerator()
{
	delete outputData_;
	outputData_ = nullptr;

	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);
}

void SolidColorImageGenerator::Process()
{
	// Nothing to process, the image is constant
}
