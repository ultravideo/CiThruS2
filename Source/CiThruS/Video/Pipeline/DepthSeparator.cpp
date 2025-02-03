#include "DepthSeparator.h"

#include <algorithm>
#include <array>

DepthSeparator::DepthSeparator(const uint16_t& frameWidth, const uint16_t& frameHeight)
	: inputFrameWidth_(frameWidth), inputFrameHeight_(frameHeight), outputFrameWidth_(frameWidth), outputFrameHeight_(frameHeight)
{
	outputSize_ = outputFrameWidth_ * outputFrameHeight_ * 4;
	outputFrame_ = new uint8_t[outputSize_];
}

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
