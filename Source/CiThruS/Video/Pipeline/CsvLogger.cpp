#include "CsvLogger.h"

#include <sstream>

CsvLogger::CsvLogger() : outputData_(nullptr), outputSize_(0), headerPushed_(false), expectedFrameNumber_(0)
{
	GetOutputPin<0>().SetFormat("csv");
	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(outputSize_);

	GetInputPin<0>().SetAcceptedFormat("binary");
}

CsvLogger::~CsvLogger()
{
	delete[] outputData_;

	outputData_ = nullptr;
	outputSize_ = 0;

	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(outputSize_);
}

void CsvLogger::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData || inputSize == 0)
	{
		GetOutputPin<0>().SetData(nullptr);
		GetOutputPin<0>().SetSize(0);

		return;
	}

	const CsvLogData* data = reinterpret_cast<const CsvLogData*>(inputData);

	if (data->frameNumber != expectedFrameNumber_)
	{
		throw std::exception("Frame dropped");
	}

	expectedFrameNumber_ = data->frameNumber + 1;

	std::stringstream stream;

	if (!headerPushed_)
	{
		stream
			<< "frameNumber" << ','
			<< "frameTimestampMs" << ','
			<< "cameraPositionXMeters" << ','
			<< "cameraPositionYMeters" << ','
			<< "cameraPositionZMeters" << ','
			<< "cameraRotationPitchDegrees" << ','
			<< "cameraRotationYawDegrees" << ','
			<< "cameraRotationRollDegrees" << ','
			<< "cameraHorizontalFieldOfViewDegrees" << ','
			<< "depthRangeMeters"
			<< std::endl;

		headerPushed_ = true;
	}

	stream
		<< data->frameNumber << ','
		<< data->frameTimestampMs << ','
		<< data->cameraPos.X << ','
		<< data->cameraPos.Y << ','
		<< data->cameraPos.Z << ','
		<< data->cameraRot.X << ','
		<< data->cameraRot.Y << ','
		<< data->cameraRot.Z << ','
		<< data->cameraVerticalFov << ','
		<< data->depthRange
		<< std::endl;

	delete[] outputData_;

	outputSize_ = stream.view().size();
	outputData_ = new uint8_t[outputSize_];

	memcpy(outputData_, stream.view().data(), outputSize_);

	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(outputSize_);
}
