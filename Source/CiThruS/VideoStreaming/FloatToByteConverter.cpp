#include "FloatToByteConverter.h"
#include "Async/ParallelFor.h"

#include <algorithm>

FloatToByteConverter::FloatToByteConverter(const uint16_t& frameWidth, const uint16_t& frameHeight, const uint8_t& threadCount)
	: inputFrameWidth_(frameWidth), inputFrameHeight_(frameHeight), outputFrameWidth_(frameWidth), outputFrameHeight_(frameHeight), colorChannels_(4), threadCount_(threadCount)
{
	outputSize_ = outputFrameWidth_ * outputFrameHeight_ * colorChannels_;
	outputFrame_ = new uint8_t[outputSize_];
}

FloatToByteConverter::~FloatToByteConverter()
{
	delete[] outputFrame_;
	outputFrame_ = nullptr;
	outputSize_ = 0;
}

void FloatToByteConverter::Process()
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
					uint32_t pixelOffset = (x + y * outputFrameWidth_) * colorChannels_;

					for (int colorChannel = 0; colorChannel < colorChannels_; colorChannel++)
					{
						outputFrame_[pixelOffset + colorChannel] = static_cast<int>(std::min(std::max(reinterpret_cast<float*>(*inputFrame_)[pixelOffset + colorChannel], 0.0f), 1.0f) * 255);
					}
				}
			}
		});
}

void FloatToByteConverter::SetInput(const IImageSource* source)
{
	inputFrame_ = source->GetOutput();
	std::string inputFormat = source->GetOutputFormat();

	if (inputFormat == "bgra32f")
	{
		outputFormat_ = "bgra";
	}
	else if (inputFormat == "rgba32f")
	{
		outputFormat_ = "rgba";
	}
}
