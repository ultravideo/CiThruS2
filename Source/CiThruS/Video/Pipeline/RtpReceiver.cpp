#include "RtpReceiver.h"
#include "IImageSource.h"
#include "Misc/Debug.h"

RtpReceiver::RtpReceiver(const std::string& ip, const int& srcPort)
{
#ifdef CITHRUS_UVGRTP_AVAILABLE
	streamSession_ = streamContext_.create_session(ip);
	stream_ = streamSession_->create_stream(srcPort, 0, RTP_FORMAT_H265, RCE_NO_SYSTEM_CALL_CLUSTERING);

	if (!stream_)
	{
		Debug::Log("Failed to create RTP stream");
	}
#endif // CITHRUS_UVGRTP_AVAILABLE
}

RtpReceiver::~RtpReceiver()
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

void RtpReceiver::Process()
{
#ifdef CITHRUS_UVGRTP_AVAILABLE
	frame_ = stream_->pull_frame(50);

	if (!frame_)
	{
		outputFrame_ = nullptr;
		outputSize_ = 0;

		return;
	}

	// TODO 0x00000001 needed at beginning?
	outputFrame_ = frame_->payload;
	outputSize_ = frame_->payload_len;
#endif // CITHRUS_UVGRTP_AVAILABLE
}
