// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for mock settings.

#ifndef RDP_TEST_UNIT_MODULES_MOCK_MOCK_SETTINGS_H_
#define RDP_TEST_UNIT_MODULES_MOCK_MOCK_SETTINGS_H_

#include <memory>

#include <QSettings>

class MockSettingsFactory
{
public:
    /// @brief Creates a new settings object that won't write to disk.
    /// @return A settings object that won't write to disk.
    static std::unique_ptr<QSettings> GetSettings();

private:
    MockSettingsFactory() = default;
};

#endif
