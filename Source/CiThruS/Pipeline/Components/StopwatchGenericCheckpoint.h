#pragma once

#include "Pipeline/Internal/PipelineFilter.h"

#include <cstdint>
#include <string>

class LoggingStopwatch;

// Logs a checkpoint on the given stopwatch when any data that is not null passes through this filter
class CITHRUS_API StopwatchGenericCheckpoint : public PipelineFilter<1, 1>
{
public:
	StopwatchGenericCheckpoint(LoggingStopwatch* stopwatch, const std::string& checkpointLabel);
	virtual ~StopwatchGenericCheckpoint();

	virtual void Process() override;
	virtual void OnInputPinsConnected() override;

protected:
	LoggingStopwatch* stopwatch_;
	std::string label_;

	bool isWhite_;
};
