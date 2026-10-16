#include "LoggingStopwatch.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>

LoggingStopwatch::LoggingStopwatch(const std::string& logPath)
	: lapStartTimestamp_(0), lapIndex_(0), lapActive_(false)
{
	if (logPath.find('\\') != std::string::npos)
	{
		throw std::runtime_error("Use forward slashes instead of backslashes in stopwatch log file path");
	}

	if (logPath != "")
	{
		std::filesystem::create_directories(logPath.substr(0, logPath.find_last_of('/')));

		logStream_ = std::ofstream(logPath, std::ios_base::out);
	}
}

LoggingStopwatch::~LoggingStopwatch()
{
	if (logStream_.is_open())
	{
		if (lapActive_)
		{
			bufferStream_ << "WARNING: Stopwatch was destroyed before lap " << lapIndex_ << " was explicitly ended" << std::endl;
		}

		logStream_ << bufferStream_.str();
		bufferStream_.str("");

		logStream_.close();
	}
}

void LoggingStopwatch::StartLap()
{
	if (lapActive_)
	{
		return;
	}

	bufferStream_ << "Lap " << lapIndex_ << " started" << std::endl;

	uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

	lapActive_ = true;
	lapStartTimestamp_ = now;
}

void LoggingStopwatch::ReachCheckpoint(const std::string& checkpointLabel)
{
	if (!lapActive_)
	{
		bufferStream_ << "ERROR: Tried to reach checkpoint " << checkpointLabel << " without an active lap. The checkpoints are most likely being triggered at unintended times" << std::endl;

		return;
	}

	uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	uint64_t timeSpent = (now - lapStartTimestamp_);

	std::string seconds = std::to_string(timeSpent / 1000000000);
	std::string remainder = std::to_string(timeSpent % 1000000000);

	// Add leading zeros to remainder
	while (remainder.size() < 9)
	{
		remainder = "0" + remainder;
	}

	bufferStream_ << "Lap " << lapIndex_ << " checkpoint " << checkpointLabel << " completed in " << seconds << "." << remainder << " s" << std::endl;
}

void LoggingStopwatch::EndLap()
{
	if (!lapActive_)
	{
		bufferStream_ << "ERROR: Tried to end lap without an active lap. The lap end is most likely being triggered at an unintended time" << std::endl;

		return;
	}

	uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	uint64_t timeSpent = (now - lapStartTimestamp_);

	std::string seconds = std::to_string(timeSpent / 1000000000);
	std::string remainder = std::to_string(timeSpent % 1000000000);

	// Add leading zeros to remainder
	while (remainder.size() < 9)
	{
		remainder = "0" + remainder;
	}

	bufferStream_ << "Lap " << lapIndex_ << " completed in " << seconds << "." << remainder << " s" << std::endl;
	logStream_ << bufferStream_.str();
	bufferStream_.str("");

	lapActive_ = false;
	lapIndex_++;
}
