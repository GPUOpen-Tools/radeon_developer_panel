// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Radeon Developer Service settings class definition

#ifndef RDP_SOURCE_SERVICE_GUI_SETTINGS_H_
#define RDP_SOURCE_SERVICE_GUI_SETTINGS_H_

#include <QColor>
#include <QMap>
#include <QSettings>

#include <qt_common/utils/qt_util.h>

/// A name/value pair structure used to save and load RDS settings.
struct Setting
{
    QString name;   ///< The key name for the setting.
    QString value;  ///< The value for the setting.
};

/// An enumeration that contains Ids for all items that need to be saved in the RDS Settings file.
enum SettingId
{
    kListenPort
};

/// A map type used to associate each setting Id with the corresponding value.
typedef QMap<SettingId, Setting> SettingsMap;

/// A class used to hold all of RDS's settings.
class Settings : public QObject
{
    Q_OBJECT
public:
    /// @brief Constructor
    /// @param [in] parent The parent object.
    explicit Settings(QObject* parent = Q_NULLPTR);

    /// @brief Apply default settings and then override them if found on disk.
    /// @return Returns true if settings were read from file, and false otherwise.
    bool Load();

    /// @brief Save settings file to disk
    void Save();

    /// @brief Add a setting to our active map if it is recognized.
    /// @param [in] setting The setting to add.
    void AddPotentialSetting(const QString& name, const QString& value);

    /// @brief Get current the RDS listen port.
    /// @return The current the RDS listen port.
    uint32_t GetListenPort() const;

    /// @brief Sets the port that RDS will use to listen for incoming connections.
    /// @param [in] listen_port The port used to listen for incoming connections.
    void SetListenPort(unsigned int listen_port);

private:
    /// @brief Provides the path to a file in the settings folder.
    /// @param settings_folder The name of the settings folder.
    /// @param settings_file The name of the file in the settings folder to return the path for.
    /// @return The path to the specified file in the settings folder.
    static QString GetSettingsFilePath(const QString& settings_folder, const QString& settings_file);

    /// @brief Initialize our table with default settings.
    void InitDefaultSettings();

    /// @brief Store an active setting.
    /// @param [in] setting_id The identifier for this setting.
    /// @param [in] setting value setting value.
    void AddActiveSetting(SettingId setting_id, const QString& value);

    /// @brief Get a setting as an integer value.
    /// @param [in] setting_id The identifier for this setting.
    /// @return The integer value for the setting specified.
    int GetIntValue(SettingId setting_id) const;

    /// @brief Returns the name for a setting.
    /// @param setting_id The setting to return the name for.
    /// @return The name of the setting.
    static QString NameForSetting(SettingId setting_id);

    QSettings   settings_;          ///< RDS settings.
    SettingsMap default_settings_;  ///< Map containing default settings.
};

#endif
