#include "SeiEmbedder.h"

SeiEmbedder::SeiEmbedder()
	: outputData_(nullptr), outputSize_(0)
{
	GetInputPin<0>().Initialize(this, "hevc");
	GetOutputPin<0>().Initialize(this, "hevc");

	GetInputPin<1>().Initialize(this, "binary");
}

SeiEmbedder::~SeiEmbedder()
{
	while (!seiQueue_.empty())
	{
		delete[] seiQueue_.front().data;
		seiQueue_.pop();
	}

	delete[] outputData_;

	outputData_ = nullptr;
	outputSize_ = 0;

	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(outputSize_);
}

void SeiEmbedder::Process()
{
	const uint8_t* seiData = GetInputPin<1>().GetData();
	uint8_t seiDataSize = GetInputPin<1>().GetSize();

	if (seiData && seiDataSize != 0)
	{
		EmbeddedData embeddedData;

		embeddedData.size = 1 + uuid_.size() + seiDataSize;
		embeddedData.data = new uint8_t[embeddedData.size];

		memcpy(embeddedData.data, &seiDataSize, 1);
		memcpy(embeddedData.data + 1, uuid_.data(), uuid_.size());
		memcpy(embeddedData.data + 1 + uuid_.size(), seiData, seiDataSize);

		// The SEI data must be queued as Kvazaar will not return frames immediately.
		// Otherwise the data will not be synchronized with the associated image
		seiQueue_.push(embeddedData);
	}

	const uint8_t* hevcData = GetInputPin<0>().GetData();
	size_t hevcDataSize = GetInputPin<0>().GetSize();

	if (!hevcData || hevcDataSize == 0)
	{
		return;
	}

	if (seiQueue_.empty())
	{
		GetOutputPin<0>().SetData(hevcData);
		GetOutputPin<0>().SetSize(hevcDataSize);

		return;
	}

	EmbeddedData data = seiQueue_.front();
	seiQueue_.pop();

	delete[] outputData_;

	outputSize_ = hevcDataSize + HEADER_SIZE + data.size;
	outputData_ = new uint8_t[outputSize_];

	memcpy(outputData_, hevcData, hevcDataSize);
	memcpy(outputData_ + hevcDataSize, header_, HEADER_SIZE);
	memcpy(outputData_ + hevcDataSize + HEADER_SIZE, data.data, data.size);

	delete[] data.data;

	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(outputSize_);
}
