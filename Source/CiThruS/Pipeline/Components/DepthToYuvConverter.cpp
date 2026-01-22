#include "DepthToYuvConverter.h"

#include <algorithm>
#include <iostream>
#include <array>

DepthToYuvConverter::DepthToYuvConverter() : outputData_(nullptr), outputSize_(0)
{
	GetInputPin<0>().Initialize(this, { "rgba", "bgra" });
	GetOutputPin<0>().Initialize(this, "yuv420");
}

DepthToYuvConverter::~DepthToYuvConverter()
{
	delete[] outputData_;
	outputData_ = nullptr;
	outputSize_ = 0;

	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(outputSize_);
}

void DepthToYuvConverter::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData)
	{
		return;
	}

	if (outputSize_ != inputSize * 3 / 8)
	{
		outputSize_ = inputSize * 3 / 8;

		delete[] outputData_;
		outputData_ = new uint8_t[outputSize_];
		GetOutputPin<0>().SetData(outputData_);
		GetOutputPin<0>().SetSize(outputSize_);

		// Fill chrominance with constant gray as it's not needed/used
		std::fill_n(outputData_ + inputSize / 4, inputSize / 8, 127);
	}

	std::transform(
		reinterpret_cast<const std::array<uint8_t, 4>*>(inputData),
		reinterpret_cast<const std::array<uint8_t, 4>*>(inputData + inputSize),
		outputData_,
		[&](const std::array<uint8_t, 4>& input)
		{
			return input[3];
		});
}
