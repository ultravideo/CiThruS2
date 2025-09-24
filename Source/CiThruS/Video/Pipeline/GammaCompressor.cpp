#include "GammaCompressor.h"

#include <algorithm>
#include <math.h>

GammaCompressor::GammaCompressor(const float& gamma) : outputData_(nullptr), outputSize_(0), gamma_(gamma)
{
	GetInputPin<0>().SetAcceptedFormats({ "rgba32f", "bgra32f" });
}

GammaCompressor::~GammaCompressor()
{
	delete[] outputData_;
	outputData_ = nullptr;
	outputData_ = 0;

	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(outputSize_);
}

void GammaCompressor::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData)
	{
		return;
	}

	if (outputSize_ != inputSize)
	{
		outputSize_ = inputSize;

		delete[] outputData_;
		outputData_ = new uint8_t[outputSize_];
		GetOutputPin<0>().SetData(outputData_);
		GetOutputPin<0>().SetSize(outputSize_);
	}

	std::transform(
		reinterpret_cast<const float*>(inputData),
		reinterpret_cast<const float*>(inputData + inputSize),
		reinterpret_cast<float*>(outputData_),
		[&](const float& input)
		{
			return pow(input, 1.0f / gamma_);
		});
}

void GammaCompressor::OnInputPinsConnected()
{
	GetOutputPin<0>().SetFormat(GetInputPin<0>().GetFormat());
}
