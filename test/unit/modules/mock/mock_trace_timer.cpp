// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for mock trace timer.

#include "mock_trace_timer.h"

void MockTraceTimer::SetInterval(uint64_t milliseconds)
{
    interval_ = milliseconds;
}

void MockTraceTimer::SetSingleShot(bool is_single_shot)
{
    is_single_shot_ = is_single_shot;
}

void MockTraceTimer::Start()
{
    is_active_ = true;
}

void MockTraceTimer::Stop()
{
    is_active_ = false;
}

void MockTraceTimer::SetOnTimerFire(const std::function<void()>& on_fire)
{
    on_fire_callback_ = on_fire;
}

void MockTraceTimer::Fire()
{
    if (on_fire_callback_)
    {
        on_fire_callback_();
    }
}

bool MockTraceTimer::IsSingleShot() const
{
    return is_single_shot_;
}

bool MockTraceTimer::IsActive() const
{
    return is_active_;
}

uint64_t MockTraceTimer::GetInterval() const
{
    return interval_;
}
