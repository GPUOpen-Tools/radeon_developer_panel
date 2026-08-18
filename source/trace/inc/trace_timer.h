// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the devtrace timer.

#ifndef RDP_SOURCE_TRACE_INC_TRACE_TIMER_H_
#define RDP_SOURCE_TRACE_INC_TRACE_TIMER_H_

#include <cstdint>
#include <functional>

namespace devtrace
{
    /// @brief A timer.
    class TraceTimer
    {
    public:
        /// @brief Destructor.
        virtual ~TraceTimer() = default;

        /// @brief Sets the interval that the timer fires on.
        /// @param [in] milliseconds The number of milliseconds that the timer should fire after.
        virtual void SetInterval(uint64_t milliseconds) = 0;

        /// @brief Sets whether or not the timer should only fire once.
        /// @param [in] is_single_shot true if the timer should only fire once after it's started, false otherwise.
        virtual void SetSingleShot(bool is_single_shot) = 0;

        /// @brief Starts the timer.
        virtual void Start() = 0;

        /// @brief Stops the timer.
        virtual void Stop() = 0;

        /// @brief Sets the callback to be called when the timer fires.
        /// @param [in] on_timeout The function to call when the timer fires.
        virtual void SetOnTimerFire(const std::function<void()>& on_fire) = 0;
    };
};  // namespace devtrace

#endif
