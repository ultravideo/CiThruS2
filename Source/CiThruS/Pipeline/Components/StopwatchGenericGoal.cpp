#include "StopwatchGenericGoal.h"
#include "Misc/LoggingStopwatch.h"

StopwatchGenericGoal::StopwatchGenericGoal(LoggingStopwatch* stopwatch)
	: stopwatch_(stopwatch)
{
	GetInputPin<0>().Initialize(this);
}

StopwatchGenericGoal::~StopwatchGenericGoal()
{
	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);
}

void StopwatchGenericGoal::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData || inputSize == 0)
	{
		GetOutputPin<0>().SetData(nullptr);
		GetOutputPin<0>().SetSize(0);

		return;
	}

	stopwatch_->EndLap();

	GetOutputPin<0>().SetData(inputData);
	GetOutputPin<0>().SetSize(inputSize);
}

void StopwatchGenericGoal::OnInputPinsConnected()
{
	GetOutputPin<0>().Initialize(this, GetInputPin<0>().GetFormat());
}
