// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for mock settings.

#include "mock_settings.h"

bool SettingsReadFunction(QIODevice& device, QSettings::SettingsMap& map)
{
    Q_UNUSED(device)
    Q_UNUSED(map)
    return true;
}

bool SettingsWriteFunction(QIODevice& device, const QSettings::SettingsMap& map)
{
    Q_UNUSED(device)
    Q_UNUSED(map)
    return true;
}

static const QSettings::Format kMockFormat = QSettings::registerFormat("mockformat", SettingsReadFunction, SettingsWriteFunction);

std::unique_ptr<QSettings> MockSettingsFactory::GetSettings()
{
    return std::unique_ptr<QSettings>(new QSettings(kMockFormat, QSettings::UserScope, "RDP", "RDP Tests"));
}
