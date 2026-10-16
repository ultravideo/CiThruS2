#pragma once

#include "Pipeline/Internal/PipelineSource.h"

#include <cstdint>

class LoggingStopwatch;

// Blinks between black and white periodically for latency measurements
class CITHRUS_API StopwatchBlinker : public PipelineSource<1>
{
public:
	StopwatchBlinker(LoggingStopwatch* stopwatch, const uint16_t& width, const uint16_t& height, const std::string& format = "rgba");
	~StopwatchBlinker();

	virtual void Process() override;

protected:
	uint8_t* frameBuffers_[2];
	uint8_t bufferIndex_;

	LoggingStopwatch* stopwatch_;
};
