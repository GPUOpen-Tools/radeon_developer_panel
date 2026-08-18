// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for a timer based on QTimer.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_MODEL_QT_TRACE_TIMER_H_
#define RDP_SOURCE_MODULES_COMMON_INC_MODEL_QT_TRACE_TIMER_H_

#include <QObject>
#include <QTimer>

#include <trace_timer.h>

class QTraceTimer : public QObject, public devtrace::TraceTimer
{
    Q_OBJECT
public:
    /// @brief Constructor.
    QTraceTimer();

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

private slots:

    /// @brief Called when the QTimer fires.
    void OnTimerFire();

private:
    QTimer                timer_;             ///< The timer that backs this object.
    std::function<void()> on_fire_callback_;  ///< The callback to call when the timer fires.
};

#endif
