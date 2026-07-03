#include "JsonLogger.h"

#include <sstream>

JsonLogger::JsonLogger() : outputData_(nullptr), outputSize_(0), headerPushed_(false)
{
	GetOutputPin<0>().Initialize(this, "json");
	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(outputSize_);

	GetInputPin<0>().Initialize(this, "binary");
}

JsonLogger::~JsonLogger()
{
	delete[] outputData_;

	outputData_ = nullptr;
	outputSize_ = 0;

	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);
}

void JsonLogger::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData || inputSize == 0)
	{
		GetOutputPin<0>().SetData(nullptr);
		GetOutputPin<0>().SetSize(0);

		return;
	}

	const JsonLogData* data = reinterpret_cast<const JsonLogData*>(inputData);

	std::stringstream stream;

	if (!headerPushed_)
	{
		stream
			<< "{" << std::endl;

		headerPushed_ = true;
	}

	stream
		<< "  \"" << data->frameNumber << "\": [" << std::endl;

	for (int i = 0; i < data->objectsDetected.size(); i++)
	{
		const JsonObjectDetection& detectedObject = data->objectsDetected[i];

		stream
			<< "    {" << std::endl
			<< "      \"class_id\": " << detectedObject.classId << "," << std::endl
			<< "      \"x_min\": " << detectedObject.xMin << "," << std::endl
			<< "      \"y_min\": " << detectedObject.yMin << "," << std::endl
			<< "      \"x_max\": " << detectedObject.xMax << "," << std::endl
			<< "      \"y_max\": " << detectedObject.yMax << "," << std::endl
			<< "      \"track_id\": " << detectedObject.trackId << "," << std::endl
			<< "      \"polygon\": [";

		for (int j = 0; j < detectedObject.polygon.size(); j++)
		{
			stream << detectedObject.polygon[j].first << ",";
			stream << detectedObject.polygon[j].second;

			if (j != detectedObject.polygon.size() - 1)
			{
				stream << ",";
			}
		}

		stream
			<< "]," << std::endl
			<< "      \"mask_color\": [" << static_cast<int>(detectedObject.maskColorR) << "," << static_cast<int>(detectedObject.maskColorG) << "," << static_cast<int>(detectedObject.maskColorB) << "]," << std::endl
			<< "      \"iscrowd\": " << detectedObject.isCrowd << std::endl
			<< "    }" << (i != data->objectsDetected.size() - 1 ? "," : "") << std::endl;
	}

	stream
		<< "  ]," << std::endl;

	// TODO this never actually finishes the JSON object with an } at the end because we don't know when the stream ends

	delete[] outputData_;

	outputSize_ = stream.str().size();
	outputData_ = new uint8_t[outputSize_];

	memcpy(outputData_, stream.str().data(), outputSize_);

	GetOutputPin<0>().SetData(outputData_);
	GetOutputPin<0>().SetSize(outputSize_);
}
