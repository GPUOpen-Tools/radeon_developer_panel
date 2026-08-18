// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for mock trace timer.

#ifndef RDP_TEST_UNIT_MODULES_MOCK_MOCK_TRACE_TIMER_H_
#define RDP_TEST_UNIT_MODULES_MOCK_MOCK_TRACE_TIMER_H_

#include <trace_timer.h>

class MockTraceTimer : public devtrace::TraceTimer
{
public:
    /// @brief Sets the interval that the timer fires on.
    /// @param [in] milliseconds The number of milliseconds that the timer should fire after.
    void SetInterval(uint64_t milliseconds) override;

    /// @brief Sets whether or not the timer should only fire once.
    /// @param [in] is_single_shot true if the timer should only fire once after it's started, false otherwise.
    void SetSingleShot(bool is_single_shot) override;

    /// @brief Starts the timer.
    void Start() override;

    /// @brief Stops the timer.
    void Stop() override;

    /// @brief Sets the callback to be called when the timer fires.
    /// @param [in] on_timeout The function to call when the timer fires.
    void SetOnTimerFire(const std::function<void()>& on_fire) override;

    /// @brief Forces the timer to fire.
    void Fire();

    /// @brief Returns whether or not the timer is a single shot.
    /// @return true if the timer was single shot, false otherwise.
    bool IsSingleShot() const;

    /// @brief Returns whether or not the timer is active.
    /// @return true if the timer is active, false otherwise.
    bool IsActive() const;

    /// @brief Gets the interval for the timer.
    /// @return The interval for the timer.
    uint64_t GetInterval() const;

private:
    std::function<void()> on_fire_callback_;        ///< The callback to call when the timer fires.
    bool                  is_single_shot_ = false;  ///< true if the timer is single shot, false otherwise.
    bool                  is_active_      = false;  ///< true if the timer is active, false otherwise.
    uint64_t              interval_;                ///< The interval for the timer.
};

#endif
