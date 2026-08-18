// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP settings widget class definition.

#ifndef RDP_SOURCE_FRONTEND_SETTINGS_WIDGET_H_
#define RDP_SOURCE_FRONTEND_SETTINGS_WIDGET_H_

#include <memory>

#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QWidget>

#include "settings_manager.h"

namespace Ui
{
    class SettingsWidget;
}

namespace rdp
{
    /// @brief RDP 'Paths' tab content widget.
    class SettingsWidget : public QWidget
    {
    public:
        /// @brief Constructor.
        /// @param [in] parent The parent widget.
        explicit SettingsWidget(QWidget* parent = nullptr);

        /// @brief Destructor.
        ~SettingsWidget() override;

        /// @brief Initializes this widget.
        /// @param [in] settings_manager The manager to use to save settings to.
        void Initialize(std::shared_ptr<SettingsManager> settings_manager);

    private slots:

        /// @brief Handle response to restore defaults clicked.
        /// @param [in] clicked Clicked state.
        void OnRestoreDefaultsClicked(bool clicked);

        /// @brief Handle browse to RGP button clicked.
        void OnBrowseToRgp();

        /// @brief Handle browse to RMV button clicked.
        void OnBrowseToRmv();

        /// @brief Handle browse to RRA button clicked.
        void OnBrowseToRra();

        /// @brief Handle browse to RGD button clicked.
        void OnBrowseToRgd();

        /// @brief Handle browse to settings file button clicked.
        void OnBrowseToSettingsFile();

        /// @brief Handle browse to text editor button clicked.
        void OnBrowseToTxtEditor();

        /// @brief Handle browsing for an application's executable.
        /// @param [in] line_edit The line edit to set the path in.
        /// @param [in] browse_text The text to display in the file chooser.
        void OnBrowseToExe(QLineEdit* line_edit, const QString& browse_text);

        /// @brief Handle browsing for an the entirety of RDTS.
        void BrowseToRdts();

        /// @brief Handle response to RGP executable path change
        /// @param [in] text The new path value.
        void OnRgpExecutablePathTextChanged(const QString& text);

        /// @brief Handle response to RMV executable path change
        /// @param [in] text The new path value.
        void OnRmvExecutablePathTextChanged(const QString& text);

        /// @brief Handle response to RRA executable path change
        /// @param [in] text The new path value.
        void OnRraExecutablePathTextChanged(const QString& text);

        /// @brief Handle response to RGD executable path change
        /// @param [in] text The new path value.
        void OnRgdExecutablePathTextChanged(const QString& text);

        /// @brief Handle response to setting file path change
        /// @param [in] text The new path value.
        void OnSettingsFilePathTextChanged(const QString& text);

        /// @brief Handle response to text editor executable path change
        /// @param [in] text The new path value.
        void OnTxtEditorExecutablePathTextChanged(const QString& text);

        /// @brief Handle response to the theme combo selection change
        /// @param [in] index The newly selected theme index.
        void OnThemeCurrentIndexChanged(int index);

        /// @brief Handle browse to RGP backend test button clicked.
        void OnBrowseToRgpBackendTest();

        /// @brief Handle browse to RMV backend test button clicked.
        void OnBrowseToRmvBackendTest();

        /// @brief Handle browse to RRA backend test button clicked.
        void OnBrowseToRraBackendTest();

        /// @brief Handle response to RGP backend test executable path change
        /// @param [in] text The new path value.
        void OnRgpBackendTestExecutablePathTextChanged(const QString& text);

        /// @brief Handle response to RMV backend test executable path change
        /// @param [in] text The new path value.
        void OnRmvBackendTestExecutablePathTextChanged(const QString& text);

        /// @brief Handle response to RRA backend test executable path change
        /// @param [in] text The new path value.
        void OnRraBackendTestExecutablePathTextChanged(const QString& text);

    private:
        /// @brief Handle the response to the path of an executable changing.
        /// @param [in] text The new path value.
        /// @param [in] key The settings key to store the new path under.
        void OnExecutablePathChanged(const QString& text, const QString& key);

    private slots:

        /// @brief Handle the auto open trace checkbox being toggled.
        /// @param state The new state of the checkbox.
        void OnAutoOpenTraceToggled(int state);

        void ValidatePaths();

    private:
        /// @brief Handle response to new color theme option selected
        /// @param [in] color_theme_option The new color theme.
        void OnColorThemeSelected(ColorThemeOption color_theme_option);

        /// @brief Gets the default path for application.
        /// @param [in] application_name The application name.
        /// @param [in] application_internal_name The internal application name.
        /// @return default application path.
        static QString QueryDefaultPath(const QString& application_name, const QString& application_internal_name);

        /// @brief Gets the path to the default text editor.
        /// @return The path to the default text editor.
        static QString GetDefaultTextEditorPath();

        /// @brief Validates RGP path.
        /// @return true if valid, false otherwise.
        bool ValidateRgpPath();

        /// @brief Validates RMV path.
        /// @return true if valid, false otherwise.
        bool ValidateRmvPath();

        /// @brief Validates RRA path.
        /// @return true if valid, false otherwise.
        bool ValidateRraPath();

        /// @brief Validates RGD path.
        /// @return true if valid, false otherwise.
        bool ValidateRgdPath();

        /// @brief Validates the settings file path.
        /// @return true if valid, false otherwise.
        bool ValidateSettingsFilePath();

        /// @brief Validates the text editor path.
        /// @return true if valid, false otherwise.
        bool ValidateTxtEditorPath();

        /// @brief Validates that a path is valid.
        /// @param [in] line_edit The line edit that has the path to validate.
        /// @param [in] status_label The label to use to display the validation status of the path.
        /// @param [in] invalid_text The text to display on the status_label if the path is invalid.
        /// @return true if the path validated, false otherwise.
        static bool ValidatePath(QLineEdit* line_edit, QLabel* status_label, const QString& invalid_text);

        /// @brief Validates RGP backend test path.
        /// @return true if valid, false otherwise.
        bool ValidateRgpBackendTestPath();

        /// @brief Validates RMV backend test path.
        /// @return true if valid, false otherwise.
        bool ValidateRmvBackendTestPath();

        /// @brief Validates RRA backend test path.
        /// @return true if valid, false otherwise.
        bool ValidateRraBackendTestPath();

        /// @brief Shows/hides backend test group box.
        void UpdateBackendTestVisibility();

        /// @brief Timer that triggers file re-validation.
        ///
        /// There does exist a class QFileSystemWatcher which watches the file system for changes.
        /// Unfortunately this class is limited in what it can do -- so it can't be used for this use case.
        ///
        /// For example, if there exists a file C:\Users\AMD\Documents\Radeon Developer Suite\RGP.exe that the watcher is looking at
        /// and the folder gets moved so it is now at C:\Users\AMD\Desktop\Radeon Developer Suite\RGP.exe, no signal will be emitted.
        ///
        /// If the watcher is also watching C:\Users\AMD\Documents\Radeon Developer Suite and then it gets moved, the watcher will also
        /// not emit a signal. In order to get the signal the watcher would also have to watch C:\Users\AMD\Documents, C:\Users\AMD,
        /// C:\Users and C:\.
        QTimer validation_timer_;

        std::unique_ptr<Ui::SettingsWidget> ui_;                ///< Qt ui.
        std::shared_ptr<SettingsManager>    settings_manager_;  ///< Manages the RDP application settings.
    };

}  // namespace rdp

#endif
