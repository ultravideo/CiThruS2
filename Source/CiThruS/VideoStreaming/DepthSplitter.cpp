#include "DepthSplitter.h"
#include "Async/ParallelFor.h"

#include <algorithm>
#include <iostream>

DepthSplitter::DepthSplitter(const uint16_t& frameWidth, const uint16_t& frameHeight, const uint8_t& threadCount)
	: inputFrameWidth_(frameWidth), inputFrameHeight_(frameHeight), outputFrameWidth_(frameWidth), outputFrameHeight_(frameHeight), threadCount_(threadCount)
{
	outputSize_ = outputFrameWidth_ * outputFrameHeight_ * 4;
	outputFrame_ = new uint8_t[outputSize_];
}

DepthSplitter::~DepthSplitter()
{
	delete[] outputFrame_;
	outputFrame_ = nullptr;
	outputSize_ = 0;
}

void DepthSplitter::Process()
{
	const int threadBatchSize = ceil(outputFrameWidth_ / (float)threadCount_);

	ParallelFor(threadCount_, [&](int32_t i)
		{
			const int threadBatchStart = threadBatchSize * i;
			const int threadBatchEnd = std::min(threadBatchSize * (i + 1), static_cast<int>(outputFrameWidth_));

			for (int32_t x = threadBatchStart; x < threadBatchEnd; x++)
			{
				for (int32_t y = 0; y < outputFrameHeight_; y++)
				{
					uint32_t pixelOffset = x + y * outputFrameWidth_;

					reinterpret_cast<float*>(outputFrame_)[pixelOffset] = reinterpret_cast<float*>(*inputFrame_)[pixelOffset * 4 + 3];
				}
			}
		});
}
