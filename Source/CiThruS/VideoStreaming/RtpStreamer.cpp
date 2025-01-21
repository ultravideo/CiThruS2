#include "VideoStreaming/RtpStreamer.h"
#include "IImageSource.h"
#include "Misc/Debug.h"

RtpStreamer::RtpStreamer(const std::string& ip, const int& dstPort)
{
#ifdef CITHRUS_UVGRTP_AVAILABLE
	streamSession_ = streamContext_.create_session(ip);
	stream_ = streamSession_->create_stream(0, dstPort, RTP_FORMAT_H265, RCE_NO_SYSTEM_CALL_CLUSTERING);

	if (!stream_)
	{
		Debug::Log("Failed to create RTP stream");
	}
#endif // CITHRUS_UVGRTP_AVAILABLE
}

RtpStreamer::~RtpStreamer()
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

void RtpStreamer::Process()
{
	if (*inputSize_ == 0)
	{
		return;
	}

#ifdef CITHRUS_UVGRTP_AVAILABLE
	if (stream_->push_frame(*inputFrame_, *inputSize_, RTP_NO_FLAGS) != RTP_ERROR::RTP_OK)
	{
		Debug::Log("Failed to push frame");
	}
#endif // CITHRUS_UVGRTP_AVAILABLE
}

void RtpStreamer::SetInput(const IImageSource* source)
{
	inputFrame_ = source->GetOutput();
	inputSize_ = source->GetOutputSize();
}
