#include "FileSink.h"

#include <filesystem>

FileSink::FileSink(const std::string& filePath)
{
	GetInputPin<0>().AcceptAnyFormat();

	if (std::filesystem::exists(filePath))
	{
		if (!std::filesystem::is_regular_file(filePath))
		{
			throw std::invalid_argument("Given path is not a file");
		}

		std::error_code error;

		std::filesystem::remove(filePath, error);

		if (error.value())
		{
			throw std::invalid_argument("Failed to remove existing file: " + error.message());
		}
	}
	else
	{
		std::filesystem::path directoryPath = std::filesystem::path(filePath).parent_path();

		if (!std::filesystem::exists(directoryPath))
		{
			std::filesystem::create_directories(directoryPath);
		}
	}

	fileHandle_ = fopen(filePath.data(), "ab");

	if (fileHandle_ == nullptr)
	{
		throw std::logic_error("Failed to open file");

		return;
	}
}

FileSink::~FileSink()
{
	fclose(fileHandle_);

	fileHandle_ = nullptr;
}

void FileSink::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData || inputSize == 0)
	{
		return;
	}

	fwrite(inputData, 1, inputSize, fileHandle_);
}
