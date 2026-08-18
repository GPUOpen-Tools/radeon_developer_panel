// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the cancellable timer.

#ifndef RDP_SOURCE_TRACE_SRC_RGP_CANCELLABLE_TIMER_H_
#define RDP_SOURCE_TRACE_SRC_RGP_CANCELLABLE_TIMER_H_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

namespace devtrace
{
    class CancellableTimer
    {
    public:
        /// @brief Destructor. Ensures any running timer is cancelled.
        ~CancellableTimer();

        /// @brief Cancels the timer.
        void Cancel();

        /// @brief Starts the timer.
        /// @param [in] time_ms The duration of the timer.
        /// @param [in] on_triggered The function to call when the timer triggers.
        /// @param [in] on_cancelled The function to call when the timer is cancelled.
        void Start(uint32_t time_ms, const std::function<void()>& on_triggered, const std::function<void()>& on_cancelled);

    private:
        std::mutex              mutex_;              ///< Mutex that guards state.
        std::condition_variable cancel_cv_;          ///< Condition variable for cancellation.
        std::atomic<bool>       cancelled_ = false;  ///< true if the timer has been cancelled.
        std::thread             timer_thread_;       ///< The thread running the timer.
    };
}  // namespace devtrace

#endif
