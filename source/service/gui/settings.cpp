// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  RDS Settings class implementation

#include "settings.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTextStream>

#include <common/inc/util.h>

#include <set>

#include "definitions.h"

static constexpr auto kServiceSettingsDirectory = "RadeonDeveloperService";  ///< Name of the directory used to read/write the RDS settings.

Settings::Settings(QObject* parent)
    : QObject(parent)
    , settings_(GetSettingsFilePath(kServiceSettingsDirectory, kProductSettingsFilename), QSettings::IniFormat)
{
    InitDefaultSettings();
}

void Settings::AddPotentialSetting(const QString& name, const QString& value)
{
    for (SettingsMap::iterator i = default_settings_.begin(); i != default_settings_.end(); ++i)
    {
        if (i.value().name.compare(name) == 0)
        {
            AddActiveSetting(i.key(), value);
            break;
        }
    }
}

bool Settings::Load()
{
    // Reload the settings file
    settings_.sync();

    // Begin by applying the defaults.
    // If the active_settings_ already has a value for a setting, ignore the default.
    for (SettingsMap::iterator i = default_settings_.begin(); i != default_settings_.end(); ++i)
    {
        if (!settings_.contains(i.value().name))
        {
            AddActiveSetting(i.key(), i.value().value);
        }
    }

    return true;
}

void Settings::Save()
{
    settings_.sync();
}

QString Settings::GetSettingsFilePath(const QString& settings_folder, const QString& settings_file)
{
    QString path = Util::GetSettingsFolder(settings_folder);
    path.append(QDir::separator());
    path.append(settings_file);

    return path;
}

void Settings::InitDefaultSettings()
{
    default_settings_[kListenPort] = {NameForSetting(kListenPort), QString::number(kDefaultConnectionPort)};
}

void Settings::AddActiveSetting(SettingId setting_id, const QString& value)
{
    settings_.setValue(NameForSetting(setting_id), value);
}

int Settings::GetIntValue(SettingId setting_id) const
{
    return settings_.value(NameForSetting(setting_id)).toInt();
}

QString Settings::NameForSetting(SettingId setting_id)
{
    switch (setting_id)
    {
    case kListenPort:
        return "ListenPort";
    default:
        return "Unknown";
    }
}

uint32_t Settings::GetListenPort() const
{
    return GetIntValue(kListenPort);
}

void Settings::SetListenPort(unsigned int listen_port)
{
    AddPotentialSetting(NameForSetting(kListenPort), QString::number(listen_port));
    Save();
}
