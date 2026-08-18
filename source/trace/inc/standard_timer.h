// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for a timer based on C++ std::chrono

#ifndef RDP_SOURCE_TRACE_INC_STANDARD_TIMER_H_
#define RDP_SOURCE_TRACE_INC_STANDARD_TIMER_H_

#include <trace_timer.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace devtrace
{
    /// @brief Defines a trace timer implemented using std::chrono
    class StandardTimer : public TraceTimer
    {
        using TraceClock      = std::chrono::steady_clock;
        using TraceClockPoint = std::chrono::time_point<TraceClock>;

    public:
        StandardTimer() = default;

        ~StandardTimer();

        void SetInterval(uint64_t milliseconds) override;

        void SetSingleShot(bool is_single_shot) override;

        void Start() override;

        void Stop() override;

        void SetOnTimerFire(const std::function<void()>& on_fire) override;

    private:
        /// @brief Progresses timer towards interval limit
        void Tick();

        std::mutex            fire_mutex_;              ///< Mutex protecting access to fire callback.
        std::function<void()> fire_;                    ///< Fire callback.
        std::thread           loop_;                    ///< Thread managing timing ticks.
        std::mutex            interval_mutex_;          ///< Mutex protecting timer interval access.
        uint64_t              ms_interval_;             ///< Timer interval in milliseconds.
        std::atomic_bool      is_single_shot_ = true;   ///< Flag if timer is single fire.
        std::atomic_bool      is_running_     = false;  ///< Flag if timer is running.
        TraceClockPoint       begin_;                   ///< Time point of interval start.
    };
}  // namespace devtrace

#endif
