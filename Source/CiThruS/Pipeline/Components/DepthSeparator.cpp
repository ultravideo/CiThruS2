#include "DepthSeparator.h"

#include <algorithm>
#include <array>

DepthSeparator::DepthSeparator() : outputData_(nullptr), outputSize_(0)
{
	GetInputPin<0>().Initialize(this, "rgba");
	GetOutputPin<0>().Initialize(this, "rgba");
}

DepthSeparator::~DepthSeparator()
{
	delete[] outputData_;
	outputData_ = nullptr;
	outputSize_ = 0;
}

void DepthSeparator::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData || inputSize == 0)
	{
		GetOutputPin<0>().SetData(nullptr);
		GetOutputPin<0>().SetSize(0);

		return;
	}

	if (outputSize_ != inputSize * 2)
	{
		outputSize_ = inputSize * 2;

		delete[] outputData_;
		outputData_ = new uint8_t[outputSize_];
	}

	std::transform(
		reinterpret_cast<const std::array<uint8_t, 4>*>(inputData),
		reinterpret_cast<const std::array<uint8_t, 4>*>(inputData + inputSize),
		reinterpret_cast<std::array<uint8_t, 4>*>(outputData_),
		[&](const std::array<uint8_t, 4>& input)
		{
			return std::array<uint8_t, 4> { input[0], input[1], input[2], 255 };
		});

	std::transform(
		reinterpret_cast<const std::array<uint8_t, 4>*>(inputData),
		reinterpret_cast<const std::array<uint8_t, 4>*>(inputData + inputSize),
		reinterpret_cast<std::array<uint8_t, 4>*>(outputData_ + inputSize),
		[&](const std::array<uint8_t, 4>& input)
		{
			return std::array<uint8_t, 4> { input[3], input[3], input[3], 255 };
		});

	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(outputSize_);
}
