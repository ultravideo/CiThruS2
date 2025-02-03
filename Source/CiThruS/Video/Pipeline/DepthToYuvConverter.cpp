#include "DepthToYuvConverter.h"
#include "Async/ParallelFor.h"

#include <algorithm>
#include <iostream>

DepthToYuvConverter::DepthToYuvConverter(const uint16_t& frameWidth, const uint16_t& frameHeight, const float& depthRange, const uint8_t& threadCount)
	: inputFrameWidth_(frameWidth), inputFrameHeight_(frameHeight), outputFrameWidth_(frameWidth), outputFrameHeight_(frameHeight), depthRange_(depthRange), threadCount_(threadCount)
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

	const int threadBatchSize = ceil(outputFrameWidth_ / (float)threadCount_);

	ParallelFor(threadCount_, [&](int32_t i)
		{
			const int threadBatchStart = threadBatchSize * i;
			const int threadBatchEnd = std::min(threadBatchSize * (i + 1), static_cast<int>(outputFrameWidth_));

			for (int32_t x = threadBatchStart; x < threadBatchEnd; x++)
			{
				for (int32_t y = 0; y < outputFrameHeight_; y++)
				{
					// Divide by 100 to convert from centimeters to meters, then scale to desired depth range and convert to 8-bit integer between 0 and 255
					uint8_t depth = std::min(std::max(reinterpret_cast<float*>(*inputFrame_)[x + y * outputFrameWidth_] * 255.0f / (100.0f * depthRange_), 0.0f), 255.0f);

					outputFrame_[(x + y * outputFrameWidth_)] = depth;
				}
			}
		});
}

bool DepthToYuvConverter::SetInput(const IImageSource* source)
{
	if (source->GetOutputFormat() != "gray32f")
	{
		return false;
	}

	inputFrame_ = source->GetOutput();

	return true;
}
