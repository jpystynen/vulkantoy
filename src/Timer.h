#ifndef CORE_TIMER_H
#define CORE_TIMER_H

// Copyright (c) 2017 Johannes Pystynen
// This code is licensed under the MIT license (MIT)

#include <chrono>
#include <ctime>

///////////////////////////////////////////////////////////////////////////////

namespace core
{

class Timer
{
public:
    Timer() = default;
    ~Timer() = default;

    Timer(const Timer&) = default;
    Timer& operator=(const Timer&) = default;
    Timer(Timer&&) = default;
    Timer& operator=(Timer&&) = default;

    void update()
    {
        using namespace std::chrono;

        const high_resolution_clock::time_point currTime = high_resolution_clock::now();
        const double deltaTimeMillis = 0.001 * (double)duration_cast<microseconds>(currTime - m_prevTime).count();

        const double timePointMillis = 0.001 * (double)duration_cast<microseconds>(currTime - m_prevFpsTime).count();
        if (timePointMillis > c_updateInterval)
        {
            fps = (float)std::round(m_fpsFrameCounter / (0.001 * timePointMillis));
            m_fpsFrameCounter = 0;
            m_prevFpsTime = currTime;
            m_fpsUpdated = true;
        }
        m_fpsFrameCounter++;

        deltaTimeSeconds = (float)(0.001 * deltaTimeMillis);
        timeSeconds += deltaTimeSeconds;
        m_prevTime = currTime;

        {
            const time_t tmpTime = system_clock::to_time_t(system_clock::now());
            tm localTm;
            localtime_s(&localTm, &tmpTime);
            year = (float)localTm.tm_year + 1900.0f;
            month = (float)localTm.tm_mon;
            day = (float)localTm.tm_mday;
            secs = (float)localTm.tm_sec;
        }
    }

    bool isFpsUpdated()
    {
        const bool val = m_fpsUpdated;
        m_fpsUpdated = false;
        return val;
    }

    float deltaTimeSeconds  = 0.0f;
    float timeSeconds       = 0.0f;
    float fps               = 0.0f;

    float year  = 0.0f;
    float month = 0.0f;
    float day   = 0.0f;
    float secs  = 0.0f;

private:
    const double c_updateInterval = 16.6 * 10.0;

    std::chrono::high_resolution_clock::time_point m_prevTime
    { std::chrono::high_resolution_clock::now() };

    std::chrono::high_resolution_clock::time_point m_prevFpsTime
    { std::chrono::high_resolution_clock::now() };

    uint32_t m_fpsFrameCounter  = 0;
    bool m_fpsUpdated           = false;

};

} // namespace

///////////////////////////////////////////////////////////////////////////////

#endif // CORE_TIMER_H
