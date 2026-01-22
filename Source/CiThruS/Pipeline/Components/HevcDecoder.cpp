#include "HevcDecoder.h"
#include "Misc/Debug.h"

HevcDecoder::HevcDecoder(const uint8_t& threadCount) : outputData_(nullptr), outputSize_(0), bufferIndex_(0)
{
#ifdef CITHRUS_OPENHEVC_AVAILABLE
    handle_ = libOpenHevcInit(threadCount, 2);

	if (libOpenHevcStartDecoder(handle_) == -1)
	{
		return;
	}

	libOpenHevcSetTemporalLayer_id(handle_, 0);
	libOpenHevcSetActiveDecoders(handle_, 0);
	libOpenHevcSetViewLayers(handle_, 0);
#endif // CITHRUS_OPENHEVC_AVAILABLE

    buffers_[0] = nullptr;
    buffers_[1] = nullptr;

    bufferSizes_[0] = 0;
    bufferSizes_[1] = 0;

    GetInputPin<0>().Initialize(this, "hevc");
    GetOutputPin<0>().Initialize(this, "yuv420");
}

HevcDecoder::~HevcDecoder()
{
#ifdef CITHRUS_OPENHEVC_AVAILABLE
    libOpenHevcClose(handle_);
#endif // CITHRUS_OPENHEVC_AVAILABLE

    delete buffers_[0];
    delete buffers_[1];

    buffers_[0] = nullptr;
    buffers_[1] = nullptr;

    bufferSizes_[0] = 0;
    bufferSizes_[1] = 0;

    outputData_ = nullptr;
    outputSize_ = 0;

    GetOutputPin<0>().SetData(outputData_);
    GetOutputPin<0>().SetSize(outputSize_);
}

void HevcDecoder::Process()
{
    const uint8_t* inputData = GetInputPin<0>().GetData();
    uint32_t inputSize = GetInputPin<0>().GetSize();

    if (!inputData || inputSize == 0)
    {
        return;
    }

#ifdef CITHRUS_OPENHEVC_AVAILABLE
    OpenHevc_Frame ohevc_frame;
    int decode_status = libOpenHevcDecode(handle_, inputData, inputSize, 0);
    int output_status = libOpenHevcGetOutput(handle_, decode_status, &ohevc_frame);

    if (output_status == 0)
    {
        outputData_ = nullptr;
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
#endif // CITHRUS_OPENHEVC_AVAILABLE

    outputData_ = buffers_[bufferIndex_];
    bufferIndex_ = (bufferIndex_ + 1) % 2;

    GetOutputPin<0>().SetData(outputData_);
    GetOutputPin<0>().SetSize(outputSize_);
}
