#include "BlinkDetector.h"

#include <algorithm>
#include <chrono>

BlinkDetector::BlinkDetector(const std::string& logPath) : isWhite_(false)
{
	if (logPath != "")
	{
		logStream_ = std::ofstream(logPath, std::ios_base::out);
	}

	GetInputPin<0>().SetAcceptedFormats({ "bgra", "rgba", "yuv420" });
}

BlinkDetector::~BlinkDetector()
{
	if (logStream_.is_open())
	{
		logStream_.close();
	}
}

void BlinkDetector::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData || inputSize == 0)
	{
		return;
	}

	outputSize_ = inputSize;
	outputFrame_ = inputData;

	bool isWhite = inputData[0] > 127;

	if (isWhite != isWhite_)
	{
		isWhite_ = isWhite;

		uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

		logStream_ << now << " " << (isWhite_ ? "white" : "black") << std::endl;
	}
}

void BlinkDetector::OnInputPinsConnected()
{
	GetOutputPin<0>().SetFormat(GetInputPin<0>().GetFormat());
}
