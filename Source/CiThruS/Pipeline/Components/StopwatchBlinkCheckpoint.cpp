#include "StopwatchBlinkCheckpoint.h"
#include "Misc/LoggingStopwatch.h"

StopwatchBlinkCheckpoint::StopwatchBlinkCheckpoint(LoggingStopwatch* stopwatch, const std::string& checkpointLabel)
	: stopwatch_(stopwatch), label_(checkpointLabel), isWhite_(false)
{
	GetInputPin<0>().Initialize(this, { "bgra", "rgba", "yuv420" });
}

StopwatchBlinkCheckpoint::~StopwatchBlinkCheckpoint()
{
	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);
}

void StopwatchBlinkCheckpoint::Process()
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

		stopwatch_->ReachCheckpoint(label_);
	}

	GetOutputPin<0>().SetData(inputData);
	GetOutputPin<0>().SetSize(inputSize);
}

void StopwatchBlinkCheckpoint::OnInputPinsConnected()
{
	GetOutputPin<0>().Initialize(this, GetInputPin<0>().GetFormat());
}
