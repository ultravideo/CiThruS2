#include "GammaCompressor.h"

#include <algorithm>
#include <math.h>

GammaCompressor::GammaCompressor(const uint16_t& frameWidth, const uint16_t& frameHeight, const float& gamma)
	: inputFrameWidth_(frameWidth), inputFrameHeight_(frameHeight), outputFrameWidth_(frameWidth), outputFrameHeight_(frameHeight), gamma_(gamma)
{
	outputSize_ = outputFrameWidth_ * outputFrameHeight_ * 4 * 4;
	outputFrame_ = new uint8_t[outputSize_];
}

GammaCompressor::~GammaCompressor()
{
	delete[] outputFrame_;
	outputFrame_ = nullptr;
	outputSize_ = 0;
}

void GammaCompressor::Process()
{
	if (*inputFrame_ == nullptr)
	{
		return;
	}

	std::transform(
		reinterpret_cast<const float*>(*inputFrame_),
		reinterpret_cast<const float*>(*inputFrame_ + *inputSize_),
		reinterpret_cast<float*>(outputFrame_),
		[&](const float& input)
		{
			return pow(input, 1.0f / gamma_);
		});
}

bool GammaCompressor::SetInput(const IImageSource* source)
{
	std::string inputFormat = source->GetOutputFormat();

	if (inputFormat != "bgra32f"
		&& inputFormat != "rgba32f")
	{
		return false;
	}

	outputFormat_ = inputFormat;
	inputFrame_ = source->GetOutput();
	inputSize_ = source->GetOutputSize();

	return true;
}
