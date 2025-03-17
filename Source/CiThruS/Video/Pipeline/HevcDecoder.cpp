#include "HevcDecoder.h"
#include "Misc/Debug.h"

HevcDecoder::HevcDecoder() : outputFrame_(nullptr), outputSize_(0), bufferIndex_(0)
{
    handle_ = libOpenHevcInit(1, 2);

	if (libOpenHevcStartDecoder(handle_) == -1)
	{
		return;
	}

	libOpenHevcSetTemporalLayer_id(handle_, 0);
	libOpenHevcSetActiveDecoders(handle_, 0);
	libOpenHevcSetViewLayers(handle_, 0);

    buffers_[0] = nullptr;
    buffers_[1] = nullptr;

    bufferSizes_[0] = 0;
    bufferSizes_[1] = 0;
}

HevcDecoder::~HevcDecoder()
{
    libOpenHevcClose(handle_);

    delete buffers_[0];
    delete buffers_[1];

    buffers_[0] = nullptr;
    buffers_[1] = nullptr;

    bufferSizes_[0] = 0;
    bufferSizes_[1] = 0;
}

void HevcDecoder::Process()
{
    if (*inputFrame_ == nullptr || *inputSize_ == 0)
    {
        return;
    }

    OpenHevc_Frame ohevc_frame;
    int decode_status = libOpenHevcDecode(handle_, *inputFrame_, *inputSize_, 0);
    int output_status = libOpenHevcGetOutput(handle_, decode_status, &ohevc_frame);

    if (output_status == 0)
    {
        outputFrame_ = nullptr;
        outputSize_ = 0;

        return;
    }

    libOpenHevcGetPictureInfo(handle_, &ohevc_frame.frameInfo);
    const int& width = ohevc_frame.frameInfo.nWidth;
    const int& height = ohevc_frame.frameInfo.nHeight;

    outputSize_ = width * height * 3 / 2;

    if (bufferSizes_[bufferIndex_] != outputSize_)
    {
        delete[] buffers_[bufferIndex_];
        buffers_[bufferIndex_] = new uint8_t[outputSize_];
    }

    // Copy Y
    for (int i = 0; i < height; i++)
    {
        memcpy(buffers_[bufferIndex_] + i * width, (uint8_t*)ohevc_frame.pvY + i * ohevc_frame.frameInfo.nYPitch, width);
    }

    // Copy U and V
    for (int i = 0; i < height / 2; i++)
    {
        memcpy(buffers_[bufferIndex_] + width * height + i * width / 2, (uint8_t*)ohevc_frame.pvU + i * ohevc_frame.frameInfo.nUPitch, width / 2);
        memcpy(buffers_[bufferIndex_] + width * height * 5 / 4 + i * width / 2, (uint8_t*)ohevc_frame.pvV + i * ohevc_frame.frameInfo.nVPitch, width / 2);
    }

    outputFrame_ = buffers_[bufferIndex_];
    bufferIndex_ = (bufferIndex_ + 1) % 2;
}

bool HevcDecoder::SetInput(const IImageSource* source)
{
	if (source->GetOutputFormat() != "hevc")
	{
		return false;
	}

	inputFrame_ = source->GetOutput();
	inputSize_ = source->GetOutputSize();

	return true;
}
