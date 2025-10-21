#pragma once

#include "PipelineFilter.h"
#include "Misc/TemplateUtility.h"

// Concatenates YUV 4:2:0 images vertically. Each image must have the same resolution
template <uint8_t N>
class ImageConcatenator : public PipelineFilter<N, 1>
{
public:
	ImageConcatenator(const uint16_t& frameWidth, const uint16_t& frameHeight)
		: inputFrameWidth_(frameWidth), inputFrameHeight_(frameHeight)
	{
		uint32_t outputSize = frameWidth * frameHeight * N * 3 / 2;
		outputData_ = new uint8_t[outputSize];

		PipelineSource<1>::GetOutputPin<0>().SetData(outputData_);
		PipelineSource<1>::GetOutputPin<0>().SetSize(outputSize);

		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			this->template GetInputPin<i>().SetAcceptedFormat("yuv420");
		});

		PipelineSource<1>::GetOutputPin<0>().SetFormat("yuv420");
	}

	~ImageConcatenator()
	{
		delete[] outputData_;
		outputData_ = nullptr;

		PipelineSource<1>::GetOutputPin<0>().SetData(outputData_);
		PipelineSource<1>::GetOutputPin<0>().SetSize(0);
	}

	virtual void Process() override
	{
		const int frameSize = inputFrameWidth_ * inputFrameHeight_;

		TemplateUtility::For<N>([&, this]<uint8_t i>()
		{
			const uint8_t* inputData = this->template GetInputPin<i>().GetData();
			uint32_t inputSize = this->template GetInputPin<i>().GetSize();

			if (!inputData || inputSize != frameSize * 3 / 2)
			{
				return;
			}

			// Copy Y
			memcpy(outputData_ + frameSize * i, inputData, frameSize);
			// Copy U
			memcpy(outputData_ + frameSize / 4 * i + frameSize * N, inputData + frameSize, frameSize / 4);
			// Copy V
			memcpy(outputData_ + frameSize / 4 * i + frameSize * 5 / 4 * N, inputData + frameSize * 5 / 4, frameSize / 4);
		});
	}

protected:
	uint8_t* outputData_;

	uint16_t inputFrameWidth_;
	uint16_t inputFrameHeight_;
};
