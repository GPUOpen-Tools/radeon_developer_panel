// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Application settings class implementation.

#include "settings_manager.h"

#include <memory>

#include <QApplication>
#include <QStandardPaths>
#include <QStyleHints>

#include "definitions.h"
#include "logging/logging_manager.h"
#include "utilities.h"

namespace
{
    ColorThemeType TypeFromOption(ColorThemeOption option)
    {
        if (option == QtCommon::QtUtils::ColorThemeOption::kSystem)
        {
            return QtCommon::QtUtils::DetectOsSetting();
        }

        return static_cast<ColorThemeType>(option);
    }
}  // namespace

namespace rdp
{
    SettingsManager::SettingsManager()
    {
        const QString settings_path = util::GetApplicationDataPath().absoluteFilePath(kSettingsFileName);
        settings_                   = std::make_unique<QSettings>(settings_path, QSettings::IniFormat);

        // Cache the Light and Dark mode stylesheets
        QFile common_style(":/common.qss");
        if (common_style.open(QIODevice::ReadOnly))
        {
            const QString common_sheet = common_style.readAll();
            light_style_sheet_         = common_sheet;
            dark_style_sheet_          = common_sheet;

            QFile light_style(":/light.qss");
            if (light_style.open(QIODevice::ReadOnly))
            {
                light_style_sheet_.append(light_style.readAll());
            }

            QFile dark_style(":/dark.qss");
            if (dark_style.open(QIODevice::ReadOnly))
            {
                dark_style_sheet_.append(dark_style.readAll());
            }
        }

        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &SettingsManager::OnColorSchemeChanged);
    }

    void SettingsManager::OnColorSchemeChanged(Qt::ColorScheme color_scheme)
    {
        Q_UNUSED(color_scheme)

        const auto color_theme_option = GetColorThemeOption();
        if (color_theme_option != ColorThemeOption::kSystem)
        {
            return;
        }

        ApplyColorThemeOption(color_theme_option);
    }

    void SettingsManager::ApplyColorThemeOption(ColorThemeOption option)
    {
        QtCommon::QtUtils::ColorTheme::Get().SetColorTheme(TypeFromOption(option));

        qApp->setPalette(GetCurrentPalette());

        ApplyStyleSheet(option);

        emit QtCommon::QtUtils::ColorTheme::Get().ColorThemeUpdated();
    }

    void SettingsManager::ApplyStyleSheet(ColorThemeOption color_theme)
    {
        if (color_theme == ColorThemeOption::kSystem)
        {
            color_theme = static_cast<ColorThemeOption>(QtCommon::QtUtils::DetectOsSetting());
        }

        if (color_theme == ColorThemeOption::kLight)
        {
            qApp->setStyleSheet(light_style_sheet_);
        }
        else if (color_theme == ColorThemeOption::kDark)
        {
            qApp->setStyleSheet(dark_style_sheet_);
        }
    }

    QPalette SettingsManager::GetCurrentPalette() const
    {
        ColorThemeOption color_theme = GetColorThemeOption();
        if (color_theme == ColorThemeOption::kSystem)
        {
            color_theme = static_cast<ColorThemeOption>(QtCommon::QtUtils::DetectOsSetting());
        }

        QPalette palette = QtCommon::QtUtils::ColorTheme::Get().GetCurrentPalette();
        if (color_theme == ColorThemeOption::kLight)
        {
            palette.setColor(QPalette::Window, QColor(240, 240, 240));
        }
        else if (color_theme == ColorThemeOption::kDark)
        {
            palette.setColor(QPalette::Base, QColor(60, 60, 60));
            palette.setColor(QPalette::AlternateBase, palette.color(QPalette::Mid));
        }

        return palette;
    }

    void SettingsManager::LoadSettings()
    {
        RDP_LOG_INFO("Loading Settings File: [{}]", qUtf8Printable(settings_->fileName()));

        settings_->sync();
    }

    void SettingsManager::SaveSettings() const
    {
        RDP_LOG_INFO("Saving Settings File: [{}]", qUtf8Printable(settings_->fileName()));

        emit SettingsSaved(settings_.get());

        settings_->sync();
    }

    ColorThemeOption SettingsManager::GetColorThemeOption() const
    {
        if (settings_ == nullptr)
        {
            return ColorThemeOption::kSystem;
        }

        const ColorThemeOption theme =
            static_cast<ColorThemeOption>(settings_->value(kApplicationSettingsThemeKey, ColorThemeType::kColorThemeTypeCount).toInt());
        return theme;
    }

    QSettings* SettingsManager::GetSettings() const
    {
        return settings_.get();
    }
}  // namespace rdp
