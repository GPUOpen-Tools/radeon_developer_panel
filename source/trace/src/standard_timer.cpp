// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for a timer based on C++ std::chrono

#include "standard_timer.h"

namespace devtrace
{
    StandardTimer::~StandardTimer() noexcept
    {
        is_running_ = false;
        if (loop_.joinable())
        {
            loop_.join();
        }
    }

    void StandardTimer::Start()
    {
        if (is_running_)
        {
            return;
        }

        is_running_ = true;
        begin_      = TraceClock::now();
        if (loop_.joinable())
        {
            loop_.join();
        }
        loop_ = std::thread([&]() { Tick(); });
    }

    void StandardTimer::Stop()
    {
        is_running_ = false;
    }

    void StandardTimer::SetSingleShot(bool is_single_shot)
    {
        is_single_shot_ = is_single_shot;
    }

    void StandardTimer::SetInterval(uint64_t milliseconds)
    {
        const std::lock_guard<std::mutex> state_lock(interval_mutex_);
        ms_interval_ = milliseconds;
    }

    void StandardTimer::SetOnTimerFire(const std::function<void()>& on_fire)
    {
        const std::lock_guard<std::mutex> state_lock(fire_mutex_);
        fire_ = on_fire;
    }

    void StandardTimer::Tick()
    {
        while (is_running_)
        {
            const auto now          = TraceClock::now();
            const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - begin_).count();
            if (milliseconds >= static_cast<int64_t>(ms_interval_))
            {
                fire_();
                if (is_single_shot_)
                {
                    Stop();
                }
                begin_ = TraceClock::now();
            }
            else
            {
                // Sleep for a portion of remaining time to reduce CPU usage while maintaining timing accuracy
                const auto remaining_ms = ms_interval_ - static_cast<uint64_t>(milliseconds);
                const auto sleep_ms     = std::max(remaining_ms / 2, static_cast<uint64_t>(1));
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            }
        }
    }
}  // namespace devtrace
