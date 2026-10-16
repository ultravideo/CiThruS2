#include "StopwatchBlinkGoal.h"
#include "Misc/LoggingStopwatch.h"

StopwatchBlinkGoal::StopwatchBlinkGoal(LoggingStopwatch* stopwatch)
	: stopwatch_(stopwatch), isWhite_(false)
{
	GetInputPin<0>().Initialize(this, { "bgra", "rgba", "yuv420" });
}

StopwatchBlinkGoal::~StopwatchBlinkGoal()
{
	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);
}

void StopwatchBlinkGoal::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData || inputSize == 0)
	{
		GetOutputPin<0>().SetData(nullptr);
		GetOutputPin<0>().SetSize(0);

		return;
	}

	bool isWhite = inputData[0] > 127;

	if (isWhite != isWhite_)
	{
		isWhite_ = isWhite;

		stopwatch_->EndLap();
	}

	GetOutputPin<0>().SetData(inputData);
	GetOutputPin<0>().SetSize(inputSize);
}

void StopwatchBlinkGoal::OnInputPinsConnected()
{
	GetOutputPin<0>().Initialize(this, GetInputPin<0>().GetFormat());
}
