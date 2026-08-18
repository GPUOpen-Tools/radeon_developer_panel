// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the cancellable timer.

#include "cancellable_timer.h"

#include <chrono>

namespace devtrace
{
    CancellableTimer::~CancellableTimer()
    {
        Cancel();
    }

    void CancellableTimer::Cancel()
    {
        {
            std::lock_guard lock(mutex_);
            cancelled_ = true;
        }
        cancel_cv_.notify_all();

        if (timer_thread_.joinable())
        {
            timer_thread_.join();
        }
    }

    void CancellableTimer::Start(const uint32_t time_ms, const std::function<void()>& on_triggered, const std::function<void()>& on_cancelled)
    {
        // Cancel any existing timer first
        Cancel();

        // Reset the cancelled flag for the new timer
        {
            std::lock_guard lock(mutex_);
            cancelled_ = false;
        }

        // Start a new timer thread
        timer_thread_ = std::thread([this, time_ms, on_triggered, on_cancelled]() {
            std::unique_lock lock(mutex_);

            // Wait for the timeout or cancellation
            const bool was_cancelled = cancel_cv_.wait_for(lock, std::chrono::milliseconds(time_ms), [this]() { return cancelled_.load(); });

            if (was_cancelled)
            {
                lock.unlock();
                on_cancelled();
            }
            else
            {
                lock.unlock();
                on_triggered();
            }
        });
    }
}  // namespace devtrace
