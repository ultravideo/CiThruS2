#include "DepthToYuvConverter.h"

#include <algorithm>
#include <iostream>

DepthToYuvConverter::DepthToYuvConverter(const uint16_t& frameWidth, const uint16_t& frameHeight, const float& depthRange)
	: inputFrameWidth_(frameWidth), inputFrameHeight_(frameHeight), outputFrameWidth_(frameWidth), outputFrameHeight_(frameHeight), depthRange_(depthRange)
{
	outputSize_ = outputFrameWidth_ * outputFrameHeight_ * 3 / 2;
	outputFrame_ = new uint8_t[outputSize_];

	// Fill chrominance with constant gray as it's not needed/used
	std::fill_n(outputFrame_ + outputFrameWidth_ * outputFrameHeight_, outputFrameWidth_ * outputFrameHeight_ / 2, 127);
}

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
