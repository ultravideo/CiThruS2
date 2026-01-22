#pragma once

#include "Optional/UvgRtp.h"
#include "Pipeline/Internal/PipelineSource.h"

#include "CoreMinimal.h"

#include <string>
#include <queue>
#include <mutex>

// Receives data from an RTP stream. Currently HEVC data only
class CITHRUS_API RtpReceiver : public PipelineSource<1>
{
public:
	RtpReceiver(const std::string& ip, const int& srcPort);
	virtual ~RtpReceiver();

	virtual void Process() override;

protected:
	bool destroyed_;

	std::mutex queueMutex_;

#ifdef CITHRUS_UVGRTP_AVAILABLE
	uvgrtp::context streamContext_;
	uvgrtp::session* streamSession_;
	uvgrtp::media_stream* stream_;

	uvgrtp::frame::rtp_frame* currentFrame_;
	std::queue<uvgrtp::frame::rtp_frame*> frameQueue_;

	static void ReceiveAsync(void* args, uvgrtp::frame::rtp_frame* frame);
#endif // CITHRUS_UVGRTP_AVAILABLE
};
