#pragma once

#include "Pipeline/Internal/PipelineFilter.h"

#include "Math/MathFwd.h"

#include <queue>

// TODO: These structs should be replaced with a generic solution that allows
// logging any kind of data
struct JsonObjectDetection
{
	int classId;
	float xMin;
	float xMax;
	float yMin;
	float yMax;
	int trackId;
	uint8_t maskColorR;
	uint8_t maskColorG;
	uint8_t maskColorB;
	int isCrowd;
	std::vector<std::pair<float, float>> polygon;
};

struct JsonLogData
{
	uint32_t frameNumber;
	std::vector<JsonObjectDetection> objectsDetected;
};

// Converts the input data into JSON
class CITHRUS_API JsonLogger : public PipelineFilter<1, 1>
{
public:
	JsonLogger();
	~JsonLogger();

	virtual void Process() override;
protected:
	uint8_t* outputData_;
	uint32_t outputSize_;

	bool headerPushed_;
};
