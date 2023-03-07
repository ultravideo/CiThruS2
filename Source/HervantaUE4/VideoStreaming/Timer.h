#pragma once

#include "CoreMinimal.h"

#include <iostream>
#include <chrono>
#include <string>
#include <vector>


class HERVANTAUE4_API Timer
{
public:
	Timer();

	void Summary() const;

	std::string Lap(const std::string& msg = "task");

	uint32_t LapMs() const;

	void Stop(const std::string& msg = "task", const bool leading_space = false);

	float Fps();

private:
	std::vector<std::chrono::time_point<std::chrono::high_resolution_clock>> m_times;
};
