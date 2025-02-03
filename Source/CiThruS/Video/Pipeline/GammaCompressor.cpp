#include "GammaCompressor.h"
#include "Async/ParallelFor.h"

#include <algorithm>
#include <math.h>

GammaCompressor::GammaCompressor(const uint16_t& frameWidth, const uint16_t& frameHeight, const uint8_t& threadCount, const float& gamma)
	: inputFrameWidth_(frameWidth), inputFrameHeight_(frameHeight), outputFrameWidth_(frameWidth), outputFrameHeight_(frameHeight), colorChannels_(4), threadCount_(threadCount), gamma_(gamma)
{
	outputSize_ = outputFrameWidth_ * outputFrameHeight_ * colorChannels_ * 4;
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

	const int threadBatchSize = ceil(outputFrameWidth_ / (float)threadCount_);

	ParallelFor(threadCount_, [&](int32_t i)
		{
			const int threadBatchStart = threadBatchSize * i;
			const int threadBatchEnd = std::min(threadBatchSize * (i + 1), static_cast<int>(outputFrameWidth_));

			for (int32_t x = threadBatchStart; x < threadBatchEnd; x++)
			{
				for (int32_t y = 0; y < outputFrameHeight_; y++)
				{
					uint32_t pixelOffset = (x + y * outputFrameWidth_) * colorChannels_;

					for (int colorChannel = 0; colorChannel < colorChannels_; colorChannel++)
					{
						reinterpret_cast<float*>(outputFrame_)[pixelOffset + colorChannel] = pow(reinterpret_cast<float*>(*inputFrame_)[pixelOffset + colorChannel], 1.0f / gamma_);
					}
				}
			}
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

	return true;
}
