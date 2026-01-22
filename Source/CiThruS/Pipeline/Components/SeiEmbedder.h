#pragma once

#include "Pipeline/Internal/PipelineFilter.h"

#include <queue>

// Embeds arbitrary data in the SEI messages of an HEVC bitstream
class CITHRUS_API SeiEmbedder : public PipelineFilter<2, 1>
{
public:
	SeiEmbedder(const std::array<uint8_t, 16>& uuid) : SeiEmbedder()
	{
		memcpy(uuid_.data(), uuid.data(), 16);
	}

	// Usage: SeiEmbedder("TestUUID12345678")
	template<uint8_t N>
	SeiEmbedder(char const(&uuid)[N]) : SeiEmbedder()
	{
		// The char array size is 1 bigger than the UUID because string literals have an extra \0 at the end
		static_assert(N == 17);

		memcpy(uuid_.data(), uuid, 16);
	}

	~SeiEmbedder();

	virtual void Process() override;

protected:
	// Start marker 4 bytes + NAL type 1 byte + payload type 1 byte
	static const uint8_t HEADER_SIZE = 4 + 1 + 1;
	
	uint8_t header_[HEADER_SIZE] =
	{
		0x00, 0x00, 0x00, 0x01, // Start marker
		0x06,                   // NAL type (SEI message)
		0x05                    // Payload type (User data unregistered)
	};

	std::array<uint8_t, 16> uuid_;

	struct EmbeddedData
	{
		uint8_t* data;
		uint32_t size;
	};

	uint8_t* outputData_;
	uint32_t outputSize_;

	std::queue<EmbeddedData> seiQueue_;

	SeiEmbedder();
};
