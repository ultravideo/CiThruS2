#include "RtpTransmitter.h"
#include "PipelineSource.h"
#include "Misc/Debug.h"

RtpTransmitter::RtpTransmitter(const std::string& ip, const int& dstPort)
{
#ifdef CITHRUS_UVGRTP_AVAILABLE
	streamSession_ = streamContext_.create_session(ip);
	// Enable generic fragmentation and pacing for smoother delivery
	stream_ = streamSession_->create_stream(0, dstPort, RTP_FORMAT_H265, RCE_FRAGMENT_GENERIC | RCE_PACE_FRAGMENT_SENDING);

	if (!stream_)
	{
		Debug::Log("Failed to create RTP stream");
	}
	else
	{
		// Configure common RTP context params
		stream_->configure_ctx(RCC_CLOCK_RATE, 90000);
		stream_->configure_ctx(RCC_MTU_SIZE, 1200); // safer MTU for typical WANs
		stream_->configure_ctx(RCC_DYN_PAYLOAD_TYPE, 96); // use PT=96 for H265
		UE_LOG(LogTemp, Log, TEXT("RtpTransmitter: Created H265 RTP stream to %s:%d (pt=96, clock-rate=90000, mtu=1200)"), *FString(ip.c_str()), dstPort);
	}
#else
	UE_LOG(LogTemp, Warning, TEXT("RtpTransmitter: uvgRTP not available at compile time; RTP streaming disabled"));
#endif // CITHRUS_UVGRTP_AVAILABLE

	GetInputPin<0>().SetAcceptedFormat("hevc");
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
	RTP_ERROR err = stream_->push_frame(const_cast<uint8_t*>(inputData), inputSize, RTP_NO_FLAGS);
	if (err != RTP_ERROR::RTP_OK)
	{
		UE_LOG(LogTemp, Error, TEXT("RtpTransmitter: push_frame failed err=%d, size=%d"), (int)err, (int)inputSize);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("RtpTransmitter: sent %d bytes"), (int)inputSize);
	}
#endif // CITHRUS_UVGRTP_AVAILABLE
}
