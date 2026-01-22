#pragma once

#include "Optional/Fpng.h"

#include "CoreMinimal.h"
#include "PipelineSink.h"

#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <array>
#include <semaphore>

// Records RGBA images into PNG files using fpng
class CITHRUS_API PngRecorder : public PipelineSink<1>
{
public:
	PngRecorder(const std::string& directory, const uint16_t& imageWidth, const uint16_t& imageHeight);
	virtual ~PngRecorder();

	virtual void Process() override;

protected:
	struct PngBufferData
	{
		uint8_t* data;
		size_t size;
		uint64_t frameIndex;
	};

	uint16_t imageWidth_;
	uint16_t imageHeight_;

	std::string directory_;

	uint64_t frameIndex_;
};
