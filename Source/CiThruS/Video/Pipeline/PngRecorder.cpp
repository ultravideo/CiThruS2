#include "PngRecorder.h"
#include "Misc/Debug.h"

#include <filesystem>

PngRecorder::PngRecorder(const std::string& directory, const uint16_t& imageWidth, const uint16_t& imageHeight)
	: imageWidth_(imageWidth), imageHeight_(imageHeight), directory_(directory), frameIndex_(0)
{
	GetInputPin<0>().SetAcceptedFormat("rgba");

	if (!std::filesystem::exists(directory_))
	{
		std::filesystem::create_directory(directory_);
	}
	else if (!std::filesystem::is_directory(directory_))
	{
		throw std::invalid_argument("Given path is a file, not a directory");
	}
}

PngRecorder::~PngRecorder()
{
	
}

void PngRecorder::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData || inputSize != imageWidth_ * imageHeight_ * 4)
	{
		return;
	}

	std::string fileName = directory_ + (directory_[directory_.size() - 1] == '/' ? "" : "/") + std::to_string(frameIndex_) + ".png";

#ifdef CITHRUS_FPNG_AVAILABLE
	fpng::fpng_encode_image_to_file(fileName.data(), inputData, imageWidth_, imageHeight_, 4, 0);
#endif // CITHRUS_FPNG_AVAILABLE

	frameIndex_++;
}
