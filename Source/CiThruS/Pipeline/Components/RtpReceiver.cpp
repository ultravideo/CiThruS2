#include "RtpReceiver.h"
#include "Misc/Debug.h"

RtpReceiver::RtpReceiver(const std::string& ip, const int& srcPort) : destroyed_(false)
{
#ifdef CITHRUS_UVGRTP_AVAILABLE
	currentFrame_ = nullptr;

	streamSession_ = streamContext_.create_session(ip);
	stream_ = streamSession_->create_stream(srcPort, 0, RTP_FORMAT_H265, RCE_NO_FLAGS);

	if (!stream_)
	{
		Debug::Log("Failed to create RTP stream");
	}

	if (stream_->install_receive_hook(this, ReceiveAsync) != RTP_OK)
	{
		Debug::Log("Failed to create RTP stream");
	}
#endif // CITHRUS_UVGRTP_AVAILABLE

	GetOutputPin<0>().Initialize(this, { "hevc" });
}

RtpReceiver::~RtpReceiver()
{
	queueMutex_.lock();

	destroyed_ = true;

#ifdef CITHRUS_UVGRTP_AVAILABLE
	if (currentFrame_)
	{
		uvgrtp::frame::dealloc_frame(currentFrame_);
		currentFrame_ = nullptr;
	}

	while (!frameQueue_.empty())
	{
		uvgrtp::frame::dealloc_frame(frameQueue_.front());

		frameQueue_.pop();
	}
	
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

	queueMutex_.unlock();

	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);
}

void RtpReceiver::Process()
{
#ifdef CITHRUS_UVGRTP_AVAILABLE
	queueMutex_.lock();

	if (!frameQueue_.empty())
	{
		if (currentFrame_)
		{
			uvgrtp::frame::dealloc_frame(currentFrame_);
		}

		currentFrame_ = frameQueue_.front();
		frameQueue_.pop();

		GetOutputPin<0>().SetData(currentFrame_->payload);
		GetOutputPin<0>().SetSize(currentFrame_->payload_len);
	}

	queueMutex_.unlock();
#endif // CITHRUS_UVGRTP_AVAILABLE
}

#ifdef CITHRUS_UVGRTP_AVAILABLE
void RtpReceiver::ReceiveAsync(void* args, uvgrtp::frame::rtp_frame* frame)
{
	if (!frame)
	{
		return;
	}

	RtpReceiver* receiver = static_cast<RtpReceiver*>(args);

	receiver->queueMutex_.lock();

	if (!receiver->destroyed_)
	{
		receiver->frameQueue_.push(frame);
	}
	else
	{
		uvgrtp::frame::dealloc_frame(frame);
	}
	
	receiver->queueMutex_.unlock();
}
#endif // CITHRUS_UVGRTP_AVAILABLE
