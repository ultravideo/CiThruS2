#include "DepthToYuvConverter.h"

#include <algorithm>
#include <iostream>

DepthToYuvConverter::~DepthToYuvConverter()
{
	delete[] outputFrame_;
	outputFrame_ = nullptr;
}

void DepthToYuvConverter::Process()
{
	if (*inputFrame_ == nullptr)
	{
		return;
	}

	if (outputSize_ != *inputSize_ * 3 / 2)
	{
		outputSize_ = *inputSize_ * 3 / 2;

		delete[] outputFrame_;
		outputFrame_ = new uint8_t[outputSize_];

		// Fill chrominance with constant gray as it's not needed/used
		std::fill_n(outputFrame_ + *inputSize_, *inputSize_ / 2, 127);
	}

	std::transform(
		reinterpret_cast<const float*>(*inputFrame_),
		reinterpret_cast<const float*>(*inputFrame_ + *inputSize_),
		outputFrame_,
		[&](const float& input)
		{
			return std::min(std::max(input * 255.0f / (100.0f * depthRange_), 0.0f), 255.0f);
		});
}

bool DepthToYuvConverter::SetInput(const IImageSource* source)
{
	if (source->GetOutputFormat() != "gray32f")
	{
		return false;
	}

	inputFrame_ = source->GetOutput();
	inputSize_ = source->GetOutputSize();

	return true;
}
