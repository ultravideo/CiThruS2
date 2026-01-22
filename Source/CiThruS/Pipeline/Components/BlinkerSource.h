#pragma once

#include "Pipeline/Internal/PipelineSource.h"

#include <vector>
#include <fstream>

// Blinks between black and white periodically for latency measurements
class CITHRUS_API BlinkerSource : public PipelineSource<1>
{
public:
	BlinkerSource(const uint16_t& width, const uint16_t& height, const double& frequency, const std::string& logPath = "", const std::string& format = "rgba");
	~BlinkerSource();

	virtual void Process() override;

protected:
	uint8_t* frameBuffers_[2];
	uint8_t bufferIndex_;

	uint32_t nsBetweenSwaps_;

	std::ofstream logStream_;
};
