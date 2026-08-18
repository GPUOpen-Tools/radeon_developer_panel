// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for a timer based on QTimer.

#include "common/inc/model/qt_trace_timer.h"

QTraceTimer::QTraceTimer()
{
    connect(&timer_, &QTimer::timeout, this, &QTraceTimer::OnTimerFire);
}

void QTraceTimer::SetInterval(uint64_t milliseconds)
{
    timer_.setInterval(static_cast<int>(milliseconds));
}

void QTraceTimer::SetSingleShot(bool is_single_shot)
{
    timer_.setSingleShot(is_single_shot);
}

void QTraceTimer::Start()
{
    timer_.start();
}

void QTraceTimer::Stop()
{
    timer_.stop();
}

void QTraceTimer::SetOnTimerFire(const std::function<void()>& on_fire)
{
    on_fire_callback_ = on_fire;
}

void QTraceTimer::OnTimerFire()
{
    if (on_fire_callback_)
    {
        on_fire_callback_();
    }
}
