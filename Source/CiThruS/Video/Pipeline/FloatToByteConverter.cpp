#include "FloatToByteConverter.h"

#include <algorithm>

FloatToByteConverter::~FloatToByteConverter()
{
	delete[] outputFrame_;
	outputFrame_ = nullptr;
	outputSize_ = 0;
}

void FloatToByteConverter::Process()
{
	if (*inputFrame_ == nullptr)
	{
		return;
	}

	if (outputSize_ != *inputSize_ / 4)
	{
		outputSize_ = *inputSize_ / 4;

		delete[] outputFrame_;
		outputFrame_ = new uint8_t[outputSize_];
	}

	std::transform(
		reinterpret_cast<const float*>(*inputFrame_),
		reinterpret_cast<const float*>(*inputFrame_ + *inputSize_),
		outputFrame_,
		[&](const float& input)
		{
			return static_cast<uint8_t>(std::min(std::max(input, 0.0f), 1.0f) * 255);
		});
}

bool FloatToByteConverter::SetInput(const IImageSource* source)
{
	std::string inputFormat = source->GetOutputFormat();

	if (inputFormat == "bgra32f")
	{
		outputFormat_ = "bgra";
	}
	else if (inputFormat == "rgba32f")
	{
		outputFormat_ = "rgba";
	}
	else
	{
		return false;
	}

	inputFrame_ = source->GetOutput();
	inputSize_ = source->GetOutputSize();

	return true;
}
