#pragma once

#include <cstdint>
#include <fstream>
#include <sstream>

// Logs timing measurements into a file
class LoggingStopwatch
{
public:
	LoggingStopwatch(const std::string& logPath);
	~LoggingStopwatch();

	// Starts timer from 0, does nothing if timer is already active
	void StartLap();
	// Logs time with given label without resetting the timer
	void ReachCheckpoint(const std::string& checkpointLabel);
	// Logs time and stops timer
	void EndLap();

	bool IsLapActive() const { return lapActive_; }

protected:
	uint64_t lapStartTimestamp_;
	uint32_t lapIndex_;
	bool lapActive_;

	// The point of the separate buffer stream is to minimize file writing so
	// that it doesn't add delay to the measurements
	std::stringstream bufferStream_;
	std::ofstream logStream_;
};
