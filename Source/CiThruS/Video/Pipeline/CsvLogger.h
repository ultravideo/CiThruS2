#pragma once

#include "Video/Pipeline/PipelineFilter.h"

#include "Math/MathFwd.h"

#include <queue>

// TODO: This struct should be replaced with a generic solution that allows
// logging any kind of data
struct CsvLogData
{
	uint32_t frameNumber;
	FVector3f cameraPos;
	FVector3f cameraRot;
	float cameraVerticalFov;
	float depthRange;
	uint64_t frameTimestampMs;
};

// Converts the input data into CSV
class CITHRUS_API CsvLogger : public PipelineFilter<1, 1>
{
public:
	CsvLogger();
	~CsvLogger();

	virtual void Process() override;
protected:
	uint8_t* outputData_;
	uint32_t outputSize_;

	bool headerPushed_;

	uint32_t expectedFrameNumber_;
};
