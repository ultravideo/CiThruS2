#pragma once

#include "Pipeline/Internal/PipelineFilter.h"

#include <cstdint>

class LoggingStopwatch;

// Stops the given stopwatch when any data that is not null passes through this filter
class CITHRUS_API StopwatchGenericGoal : public PipelineFilter<1, 1>
{
public:
	StopwatchGenericGoal(LoggingStopwatch* stopwatch);
	virtual ~StopwatchGenericGoal();

	virtual void Process() override;
	virtual void OnInputPinsConnected() override;

protected:
	LoggingStopwatch* stopwatch_;
};
