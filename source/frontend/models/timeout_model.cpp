// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementatino for model that handles timeouts for DevDriver.

#include "timeout_model.h"

#include <QSettings>

#include "settings_manager.h"

namespace rdp
{
    static constexpr const char* kRetryTimeoutKey         = "dd_retry_timeout";
    static constexpr const char* kCommunicationTimeoutKey = "dd_communication_timeout";
    static constexpr const char* kConnectionTimeoutKey    = "dd_connection_timeout";

    TimeoutModel::TimeoutModel(const std::shared_ptr<class SettingsManager>& settings_manager)
        : settings_manager_(settings_manager)
    {
    }

    CustomDDTimeouts TimeoutModel::GetSavedTimeouts()
    {
        QSettings* settings = settings_manager_->GetSettings();
        if (settings == nullptr)
        {
            return {};
        }

        CustomDDTimeouts timeouts{};
        timeouts.retry_timeout_ms         = settings->value(kRetryTimeoutKey, 0).toLongLong();
        timeouts.communication_timeout_ms = settings->value(kCommunicationTimeoutKey, 0).toLongLong();
        timeouts.connection_timeout_ms    = settings->value(kConnectionTimeoutKey, 0).toLongLong();

        return timeouts;
    }

    void TimeoutModel::SetTimeouts(const CustomDDTimeouts& timeouts)
    {
        QSettings* settings = settings_manager_->GetSettings();
        if (settings == nullptr)
        {
            return;
        }

        settings->setValue(kRetryTimeoutKey, timeouts.retry_timeout_ms);
        settings->setValue(kCommunicationTimeoutKey, timeouts.communication_timeout_ms);
        settings->setValue(kConnectionTimeoutKey, timeouts.connection_timeout_ms);

        settings->sync();

        emit TimeoutsChanged();
    }

    void TimeoutModel::ResetTimeouts()
    {
        SetTimeouts({});
    }
}  // namespace rdp
