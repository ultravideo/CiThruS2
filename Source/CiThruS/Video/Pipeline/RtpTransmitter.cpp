#include "RtpTransmitter.h"
#include "PipelineSource.h"
#include "Misc/Debug.h"
#include <vector>

RtpTransmitter::RtpTransmitter(const std::string& ip, const int& dstPort)
{
#ifdef CITHRUS_UVGRTP_AVAILABLE
	streamSession_ = streamContext_.create_session(ip);
	// IMPORTANT: For H.265, uvgRTP has built-in RFC 7798 packetization.
	// By default, uvgRTP expects Annex-B bytestream (with 0x00000001 or 0x000001 start codes)
	// and will automatically:
	//   1) Parse NAL units by finding start codes
	//   2) Remove start codes
	//   3) Apply proper RFC 7798 H.265 RTP packetization (Single NAL, Aggregation, or Fragmentation)
	// 
	// DO NOT use RCE_FRAGMENT_GENERIC for H.265! It's for generic payloads that don't have
	// format-specific packetization (like raw audio). Using it for H.265 breaks proper NAL handling.
	// 
	// Use RCE_PACE_FRAGMENT_SENDING to spread fragments within frame interval for smoother delivery.
	stream_ = streamSession_->create_stream(0, dstPort, RTP_FORMAT_H265, RCE_PACE_FRAGMENT_SENDING);

	if (!stream_)
	{
		Debug::Log("Failed to create RTP stream");
	}
	else
	{
		// Configure common RTP context params
		stream_->configure_ctx(RCC_CLOCK_RATE, 90000);
		// Reduce MTU significantly for testing - prevents issues with large NAL units
		// Standard Ethernet MTU is 1500, but we use 1000 to be conservative for localhost testing
		stream_->configure_ctx(RCC_MTU_SIZE, 1000);
		stream_->configure_ctx(RCC_DYN_PAYLOAD_TYPE, 96); // use PT=96 for H265
		
		// Increase UDP buffer sizes to prevent packet drops
		stream_->configure_ctx(RCC_UDP_RCV_BUF_SIZE, 8 * 1024 * 1024); // 8 MB
		stream_->configure_ctx(RCC_UDP_SND_BUF_SIZE, 8 * 1024 * 1024); // 8 MB
		
		UE_LOG(LogTemp, Log, TEXT("RtpTransmitter: Created H265 RTP stream to %s:%d (pt=96, clock-rate=90000, mtu=1000, RFC7798, 8MB buffers)"), *FString(ip.c_str()), dstPort);
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
	// Parse Annex-B stream and send each NAL unit individually
	// This is CRITICAL for proper H.265 RTP packetization
	std::vector<std::pair<const uint8_t*, size_t>> nalUnits;
	size_t offset = 0;
	
	while (offset < inputSize)
	{
		// Find start code
		int startCodeLen = 0;
		if (offset + 3 <= inputSize && 
			inputData[offset] == 0x00 && inputData[offset+1] == 0x00 && inputData[offset+2] == 0x01)
		{
			startCodeLen = 3;
		}
		else if (offset + 4 <= inputSize && 
				 inputData[offset] == 0x00 && inputData[offset+1] == 0x00 && 
				 inputData[offset+2] == 0x00 && inputData[offset+3] == 0x01)
		{
			startCodeLen = 4;
		}
		else
		{
			break; // No start code found
		}
		
		size_t nalStart = offset + startCodeLen;
		
		// Find next start code
		size_t nextOffset = nalStart;
		while (nextOffset < inputSize - 2)
		{
			if (inputData[nextOffset] == 0x00 && inputData[nextOffset+1] == 0x00)
			{
				if (inputData[nextOffset+2] == 0x01 || 
					(nextOffset < inputSize - 3 && inputData[nextOffset+2] == 0x00 && inputData[nextOffset+3] == 0x01))
				{
					break;
				}
			}
			nextOffset++;
		}
		
		size_t nalEnd = (nextOffset < inputSize - 2) ? nextOffset : inputSize;
		size_t nalSize = nalEnd - nalStart;
		
		if (nalSize > 0)
		{
			nalUnits.push_back({inputData + nalStart, nalSize});
		}
		
		offset = nalEnd;
	}
	
	// Log summary
	static int framesSinceLog = 0;
	bool isKeyframe = (inputSize >= 8 && inputData[4] == 0x40); // VPS starts with 0x40
	bool shouldLog = (isKeyframe || framesSinceLog >= 30);
	
	if (shouldLog)
	{
		UE_LOG(LogTemp, Log, TEXT("RtpTransmitter: Parsed %d NAL units from %d bytes%s"),
			(int)nalUnits.size(), (int)inputSize, isKeyframe ? TEXT(" [KEYFRAME]") : TEXT(""));
		framesSinceLog = 0;
	}
	framesSinceLog++;
	
	// Send each NAL unit individually (without start codes)
	// uvgRTP will handle RFC 7798 packetization (Single NAL, AP, or FU)
	int nalIndex = 0;
	for (const auto& nal : nalUnits)
	{
		// Extract NAL type for logging
		uint8_t nalType = (nal.second > 0) ? ((nal.first[0] & 0x7E) >> 1) : 0xFF;
		
		if (shouldLog && nalIndex < 3) // Log first 3 NALs of logged frames
		{
			UE_LOG(LogTemp, Log, TEXT("  Sending NAL #%d: type=%d, size=%d"), nalIndex, nalType, (int)nal.second);
		}
		
		// Use RTP_NO_H26X_SCL flag to indicate we're providing NAL units without start codes
		RTP_ERROR err = stream_->push_frame(const_cast<uint8_t*>(nal.first), nal.second, RTP_NO_H26X_SCL);
		if (err != RTP_ERROR::RTP_OK)
		{
			UE_LOG(LogTemp, Error, TEXT("RtpTransmitter: push_frame failed err=%d, nalType=%d, nalSize=%d"), (int)err, nalType, (int)nal.second);
			break;
		}
		nalIndex++;
	}
#endif // CITHRUS_UVGRTP_AVAILABLE
}
