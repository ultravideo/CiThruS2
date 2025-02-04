#include "DepthSeparator.h"

#include <algorithm>
#include <array>

DepthSeparator::~DepthSeparator()
{
	delete[] outputFrame_;
	outputFrame_ = nullptr;
	outputSize_ = 0;
}

void DepthSeparator::Process()
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
		reinterpret_cast<const std::array<float, 4>*>(*inputFrame_),
		reinterpret_cast<const std::array<float, 4>*>(*inputFrame_ + *inputSize_),
		reinterpret_cast<float*>(outputFrame_),
		[&](const std::array<float, 4>& input)
		{
			return input[3];
		});
}

bool DepthSeparator::SetInput(const IImageSource* source)
{
	if (source->GetOutputFormat() != "rgba32f")
	{
		return false;
	}

	inputFrame_ = source->GetOutput();
	inputSize_ = source->GetOutputSize();

	return true;
}
