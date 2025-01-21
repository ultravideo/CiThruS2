#include "ImageConcatenator.h"
#include "Async/ParallelFor.h"

#include <algorithm>

ImageConcatenator::ImageConcatenator(std::vector<IImageSource*> sources, const uint16_t& frameWidth, const uint16_t& frameHeight, const uint8_t& threadCount)
	: inputFrameWidth_(frameWidth), inputFrameHeight_(frameHeight), outputFrameWidth_(frameWidth), outputFrameHeight_(frameHeight * sources.size()), threadCount_(threadCount)
{
	outputSize_ = outputFrameWidth_ * outputFrameHeight_ * 3 / 2;
	outputFrame_ = new uint8_t[outputSize_];

	sources.reserve(sources.size());

	for (IImageSource* source : sources)
	{
		inputFrames_.push_back(source->GetOutput());
	}
}

ImageConcatenator::~ImageConcatenator()
{
	delete[] outputFrame_;
	outputFrame_ = nullptr;
	outputSize_ = 0;
}

void ImageConcatenator::Process()
{
	const int frameSize = inputFrameWidth_ * inputFrameHeight_;

	for (int i = 0; i < inputFrames_.size(); i++)
	{
		// Copy Y
		memcpy(outputFrame_ + frameSize * i, *inputFrames_[i], frameSize);
		// Copy U
		memcpy(outputFrame_ + frameSize / 4 * i + frameSize * inputFrames_.size(), *inputFrames_[i] + frameSize, frameSize / 4);
		// Copy V
		memcpy(outputFrame_ + frameSize / 4 * i + frameSize * 5 / 4 * inputFrames_.size(), *inputFrames_[i] + frameSize * 5 / 4, frameSize / 4);
	}

	/*const int threadBatchSize = ceil(inputFrameWidth_ / (float)threadCount_);

	ParallelFor(threadCount_, [&](int32_t i)
		{
			const int threadBatchStart = threadBatchSize * i;
			const int threadBatchEnd = std::min(threadBatchSize * (i + 1), static_cast<int>(inputFrameWidth_));

			for (int32_t j = 0; j < inputFrames_.size(); j++)
			{
				uint32_t frameOffset = j * inputFrameWidth_ * inputFrameHeight_ * 4;

				for (int32_t x = threadBatchStart; x < threadBatchEnd; x++)
				{
					for (int32_t y = 0; y < inputFrameHeight_; y++)
					{
						uint32_t pixelOffset = (x + y * inputFrameWidth_) * 4;

						for (uint8_t channelOffset = 0; channelOffset < 4; channelOffset++)
						{
							outputFrame_[frameOffset + pixelOffset + channelOffset] = (*inputFrames_[j])[pixelOffset + channelOffset];
						}
					}
				}
			}
		});*/
}
