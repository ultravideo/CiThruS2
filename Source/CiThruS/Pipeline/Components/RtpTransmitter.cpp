#include "RtpTransmitter.h"
#include "Misc/Debug.h"

RtpTransmitter::RtpTransmitter(const std::string& ip, const int& dstPort, const int& frameRate)
{
#ifdef CITHRUS_UVGRTP_AVAILABLE
	streamSession_ = streamContext_.create_session(ip);
	stream_ = streamSession_->create_stream(0, dstPort, RTP_FORMAT_H265, RCE_SEND_ONLY | RCE_PACE_FRAGMENT_SENDING);

	if (!stream_)
	{
		Debug::Log("Failed to create RTP stream");
	}
	else if (frameRate > 0)
	{
		stream_->configure_ctx(RCC_FPS_NUMERATOR, frameRate);
		stream_->configure_ctx(RCC_FPS_DENOMINATOR, 1);
	}
#endif // CITHRUS_UVGRTP_AVAILABLE

	GetInputPin<0>().Initialize(this, "hevc");
}

RtpTransmitter::~RtpTransmitter()
{
#ifdef CITHRUS_UVGRTP_AVAILABLE
	if (stream_)
	{
		streamSession_->destroy_stream(stream_);
		stream_ = nullptr;
	}

	if (streamSession_)
	{
		streamContext_.destroy_session(streamSession_);
		streamSession_ = nullptr;
	}
#endif // CITHRUS_UVGRTP_AVAILABLE
}

void RtpTransmitter::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	size_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData || inputSize == 0)
	{
		return;
	}

#ifdef CITHRUS_UVGRTP_AVAILABLE
	// This const_cast should be okay as there should be no reason for uvgRTP to ever modify the input data
	if (stream_->push_frame(const_cast<uint8_t*>(inputData), inputSize, RTP_NO_FLAGS) != RTP_ERROR::RTP_OK)
	{
		Debug::Log("Failed to push frame");
	}
#endif // CITHRUS_UVGRTP_AVAILABLE
}
