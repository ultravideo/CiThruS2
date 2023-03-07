#include "Timer.h"
#include "../Debug.h"

Timer::Timer() : m_times({ std::chrono::high_resolution_clock::now() })
{

}

void Timer::Summary() const
{
    throw std::runtime_error("not fixed for UE4");

    std::cout << m_times.size() << " laps taken\n";
    for (int i = 1; i < m_times.size(); i++)
    {
        std::cout << "lap " << i << " took: " <<
            std::chrono::duration_cast<std::chrono::milliseconds>(m_times[i] - m_times[i - 1]).count() << " ms\n";
    }

    std::cout << "to a total time of " << std::chrono::duration_cast<std::chrono::milliseconds>(m_times.back() - m_times.front()).count() << " ms\n";
}

std::string Timer::Lap(const std::string& msg)
{
    return "execution of " + msg + " took " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::high_resolution_clock::now() - m_times.back()).count()) + " ms, total " +
        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
            (std::chrono::high_resolution_clock::now() - m_times.front()).count()) + " ms\n";

    m_times.push_back(std::chrono::high_resolution_clock::now());
}

uint32_t Timer::LapMs() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::high_resolution_clock::now() - m_times.back()).count();
}

void Timer::Stop(const std::string& msg, const bool leading_space)
{
    const auto stop_time = std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::high_resolution_clock::now() - m_times.back()).count();

    // if (std::chrono::duration_cast<std::chrono::milliseconds>
    //             (std::chrono::high_resolution_clock::now() - m_times.back()).count() > 3000)
    //     throw std::runtime_error("took too long, aborting");

    const std::string leading = leading_space ? "   " : "";

    if (m_times.size() > 1)
    {
        const std::string print_str = leading + "execution of " + msg + " since last took " +
            std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
            (std::chrono::high_resolution_clock::now() - m_times.back()).count()) + " ms, time since start " + 
            std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
            (std::chrono::high_resolution_clock::now() - m_times.front()).count()) + " ms";

        Debug::Log(FString(print_str.c_str()));
    }
    else
    {
        const std::string print_str = leading + "execution of " + msg + " since last took " + std::to_string(stop_time) + " ms";
        Debug::Log(FString(print_str.c_str()));
    }

    m_times.push_back(std::chrono::high_resolution_clock::now());
}

float Timer::Fps()
{
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::high_resolution_clock::now() - m_times.back()).count();

    return 1000.0 / elapsed_ms;
}
