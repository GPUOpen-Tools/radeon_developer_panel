// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Timer class for windows & Linux.

#ifndef RDP_SOURCE_API_EXAMPLES_TIMER_H_
#define RDP_SOURCE_API_EXAMPLES_TIMER_H_

#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

class Timer
{
public:
    Timer()
    {
        StartTimer();
    }

    ~Timer()
    {
    }

    void StartTimer()
    {
        start_time = GetCurrentTime();
    }

    bool TimeUp(uint64_t time_in_ms)
    {
        uint64_t elapsed_time = GetCurrentTime() - start_time;
        if (elapsed_time > time_in_ms)
        {
            return true;
        }
        return false;
    }

private:
    /// Get the current time in milliseconds
    uint64_t GetCurrentTime()
    {
        uint64_t current_time = 0;

#ifdef WIN32
        LARGE_INTEGER perf_time;
        LARGE_INTEGER frequency;

        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&perf_time);

        // convert to microsecond time
        perf_time.QuadPart *= 1000000;
        perf_time.QuadPart /= frequency.QuadPart;

        current_time = perf_time.QuadPart;

        // convert to milliseconds
        current_time /= 1000;
#else
        struct timeval tv;
        int            result = gettimeofday(&tv, NULL);

        // convert struct to microsecond time
        current_time = static_cast<uint64_t>((tv.tv_sec * 1000000) + tv.tv_usec);

        // convert to milliseconds
        current_time /= 1000;
#endif

        return current_time;
    }

    uint64_t start_time;  ///< The start time in milliseconds
};

#endif
