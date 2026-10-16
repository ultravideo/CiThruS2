#include "FileSink.h"

#include <filesystem>

FileSink::FileSink(const std::string& filePath)
{
	GetInputPin<0>().Initialize(this);

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

	fileStream_ = std::ofstream(filePath, std::ios_base::binary | std::ios_base::app | std::ios_base::out);

	if (!fileStream_.is_open())
	{
		throw std::logic_error("Failed to open file");

		return;
	}
}

FileSink::~FileSink()
{
	fileStream_.close();
}

void FileSink::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	if (!inputData || inputSize == 0)
	{
		return;
	}

	fileStream_.write(reinterpret_cast<const char*>(inputData), inputSize);
}
