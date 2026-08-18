// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for model that handles timeouts for DevDriver.

#ifndef RDP_SOURCE_FRONTEND_MODELS_TIMEOUT_MODEL_H_
#define RDP_SOURCE_FRONTEND_MODELS_TIMEOUT_MODEL_H_

#include <cstdint>
#include <memory>

#include <QObject>

namespace rdp
{
    /// @brief Collection of custom DD timeouts.
    struct CustomDDTimeouts
    {
        // We use longs here so that we can differentiate between -1 and UINT32_MAX

        /// @brief Retry timeout in milliseconds.
        long long retry_timeout_ms = 0;

        /// Communication timeout in milliseconds.
        long long communication_timeout_ms = 0;

        /// Connection timeout in milliseconds.
        long long connection_timeout_ms = 0;
    };

    /// @brief Model that handles the custom timeouts for DevDriver.
    class TimeoutModel : public QObject
    {
        Q_OBJECT

    public:
        /// @brief Constructor.
        /// @param [in] settings_manager The object that manages the application settings.
        TimeoutModel(const std::shared_ptr<class SettingsManager>& settings_manager);

        /// @brief Gets the custom timeouts that are saved in the settings file.
        /// @return The timeouts that were saved in the settings file.
        CustomDDTimeouts GetSavedTimeouts();

        /// @brief Sets the timeouts saved in the settings.
        /// @param [in] timeouts The timeouts to save.
        void SetTimeouts(const CustomDDTimeouts& timeouts);

        /// @brief Resets the timeouts.
        void ResetTimeouts();

    signals:
        /// @brief Emitted when the saved timeouts change.
        void TimeoutsChanged();

    private:
        std::shared_ptr<SettingsManager> settings_manager_;  ///< The object that manages the application settings.
    };
}  // namespace rdp

#endif
