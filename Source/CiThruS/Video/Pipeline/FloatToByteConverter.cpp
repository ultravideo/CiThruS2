#include "FloatToByteConverter.h"

#include <algorithm>

FloatToByteConverter::FloatToByteConverter() : outputData_(nullptr), outputSize_(0)
{
	GetInputPin<0>().SetAcceptedFormats({ "rgba32f", "bgra32f" });
}

FloatToByteConverter::~FloatToByteConverter()
{
	delete[] outputData_;
	outputData_ = nullptr;
	outputSize_ = 0;

	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(outputSize_);
}

void FloatToByteConverter::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData)
	{
		return;
	}

	if (outputSize_ != inputSize / 4)
	{
		outputSize_ = inputSize / 4;

		delete[] outputData_;
		outputData_ = new uint8_t[outputSize_];
		GetOutputPin<0>().SetData(outputData_);
		GetOutputPin<0>().SetSize(outputSize_);
	}

	std::transform(
		reinterpret_cast<const float*>(inputData),
		reinterpret_cast<const float*>(inputData + inputSize),
		outputData_,
		[&](const float& input)
		{
			return static_cast<uint8_t>(std::min(std::max(input, 0.0f), 1.0f) * 255);
		});
}

void FloatToByteConverter::OnInputPinsConnected()
{
	std::string inputFormat = GetInputPin<0>().GetFormat();

	if (inputFormat == "bgra32f")
	{
		GetOutputPin<0>().SetFormat("bgra");
	}
	else if (inputFormat == "rgba32f")
	{
		GetOutputPin<0>().SetFormat("rgba");
	}
	else
	{
		throw std::exception("Unsupported format");
	}
}
