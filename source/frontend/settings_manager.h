// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Application settings class declaration.

#ifndef RDP_SOURCE_FRONTEND_SETTINGS_MANAGER_H_
#define RDP_SOURCE_FRONTEND_SETTINGS_MANAGER_H_

#include <memory>

#include <QColor>
#include <QSettings>
#include <QString>

#include <qt_common/utils/common_definitions.h>

using ColorThemeOption = QtCommon::QtUtils::ColorThemeOption;

namespace rdp
{
    /// @brief Wraps a QSettings object to provide the settings to RDP.
    class SettingsManager : public QObject
    {
        Q_OBJECT
    public:
        /// @brief Constructor.
        SettingsManager();

        /// @brief Gets direct access to the settings storage.
        /// @return The storage for the settings.
        [[nodiscard]] QSettings* GetSettings() const;

        /// @brief Load settings.
        void LoadSettings();

        /// @brief Save settings.
        void SaveSettings() const;

        /// @brief Applies the color theme
        /// @param [in] option The color theme
        void ApplyColorThemeOption(ColorThemeOption option);

        /// @brief Applies the stylesheet for theme option
        /// @param [in] color_theme The color theme option
        void ApplyStyleSheet(ColorThemeOption color_theme);

        /// @brief Gets the active color theme option
        /// @return color theme option
        [[nodiscard]] ColorThemeOption GetColorThemeOption() const;

    signals:

        /// @brief Settings saved signal.
        /// @param [in] The settings that were saved.
        void SettingsSaved(QSettings* settings) const;

    private slots:
        /// @brief Handle response to color scheme change
        /// This slot will be called when the user switches
        /// the system color theme from Light or Dark modes
        /// @param [in] color_scheme The new color scheme
        void OnColorSchemeChanged(Qt::ColorScheme color_scheme);

    private:
        /// @brief Gets the palette for current theme
        /// @return palette for current theme
        [[nodiscard]] QPalette GetCurrentPalette() const;

        QString                    light_style_sheet_;   ///< Light mode style sheet
        QString                    dark_style_sheet_;    ///< Datk mode style sheet
        std::unique_ptr<QSettings> settings_ = nullptr;  ///< Settings storage.

        constexpr static char const* kSettingsFileName = "settings.ini";  ///< Settings filename.
    };

}  // namespace rdp

#endif
