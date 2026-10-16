#pragma once

#include "Pipeline/Internal/PipelineSink.h"

#include "CoreMinimal.h"

#include <string>

// Publishes raw image data using DDS
class CITHRUS_API DdsSink : public PipelineSink<1>
{
public:
	DdsSink(const std::string& topic,
		const uint16_t& frameWidth, const uint16_t& frameHeight,
		const float& verticalFov);
	virtual ~DdsSink();

	virtual void Process() override;
	virtual void OnInputPinsConnected() override;

protected:
	std::string topic_;

	uint16_t frameWidth_;
	uint16_t frameHeight_;
	uint16_t frameStride_;

	float verticalFov_;
	float aspectRatio_;

	uint32_t frameIndex_;

	std::string rosImageFormat_;
	bool isBigEndian_;
};
