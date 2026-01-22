#include "BgraToRgbaConverter.h"

#include <algorithm>
#include <iostream>
#include <array>

BgraToRgbaConverter::BgraToRgbaConverter() : outputData_(nullptr), outputSize_(0)
{
	GetInputPin<0>().Initialize(this, { "rgba", "bgra" });
}

BgraToRgbaConverter::~BgraToRgbaConverter()
{
	delete[] outputData_;
	outputData_ = nullptr;
	outputSize_ = 0;

	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(outputSize_);
}

void BgraToRgbaConverter::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData || inputSize == 0)
	{
		GetOutputPin<0>().SetData(nullptr);
		GetOutputPin<0>().SetSize(0);

		return;
	}

	if (outputSize_ != inputSize)
	{
		outputSize_ = inputSize;

		delete[] outputData_;
		outputData_ = new uint8_t[outputSize_];
	}

	std::transform(
		reinterpret_cast<const std::array<uint8_t, 4>*>(inputData),
		reinterpret_cast<const std::array<uint8_t, 4>*>(inputData + inputSize),
		reinterpret_cast<std::array<uint8_t, 4>*>(outputData_),
		[&](const std::array<uint8_t, 4>& input)
		{
			return std::array<uint8_t, 4> { input[2], input[1], input[0], 255 };
		});

	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(outputSize_);
}

void BgraToRgbaConverter::OnInputPinsConnected()
{
	std::string inputFormat = GetInputPin<0>().GetFormat();

	if (inputFormat == "rgba")
	{
		GetOutputPin<0>().Initialize(this, "bgra");
	}
	else if (inputFormat == "bgra")
	{
		GetOutputPin<0>().Initialize(this, "rgba");
	}
	else
	{
		throw std::runtime_error("Invalid format");
	}
}
