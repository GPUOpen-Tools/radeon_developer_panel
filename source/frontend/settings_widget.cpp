// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP settings widget class implementation.

#include "settings_widget.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QLayout>
#include <QProcess>
#include <QSettings>
#include <QStyleHints>
#include <QVBoxLayout>

#include <qt_common/utils/qt_util.h>

#include "ui_settings_widget.h"

#include "definitions.h"
#include "utilities.h"

static const constexpr char* kBrowseRgpText = "Choose path to Radeon GPU Profiler...";
#ifdef Q_OS_WIN32
static const constexpr char* kRgpApplicationName         = "RadeonGPUProfiler.exe";
static const constexpr char* kRgpApplicationNameInternal = "RadeonGPUProfiler-Internal.exe";
#else
static const constexpr char* kRgpApplicationName         = "RadeonGPUProfiler";
static const constexpr char* kRgpApplicationNameInternal = "RadeonGPUProfiler-Internal";
#endif
static const constexpr char* kRgpExecutablePathKey = "rgp_path";

static const constexpr char* kBrowseRmvText = "Choose path to Radeon Memory Visualizer...";
#ifdef Q_OS_WIN32
static const constexpr char* kRmvApplicationName         = "RadeonMemoryVisualizer.exe";
static const constexpr char* kRmvApplicationNameInternal = "RadeonMemoryVisualizer-Internal.exe";
#else
static const constexpr char* kRmvApplicationName         = "RadeonMemoryVisualizer";
static const constexpr char* kRmvApplicationNameInternal = "RadeonMemoryVisualizer-Internal";
#endif
static const constexpr char* kRmvExecutablePathKey = "rmv_path";

static const constexpr char* kBrowseRraText = "Choose path to Radeon Raytracing Analyzer...";
#ifdef Q_OS_WIN32
static const constexpr char* kRraApplicationName         = "RadeonRaytracingAnalyzer.exe";
static const constexpr char* kRraApplicationNameInternal = "RadeonRaytracingAnalyzer-Internal.exe";
#else
static const constexpr char* kRraApplicationName         = "RadeonRaytracingAnalyzer";
static const constexpr char* kRraApplicationNameInternal = "RadeonRaytracingAnalyzer-Internal";
#endif
static const constexpr char* kRraExecutablePathKey = "rra_path";

static const constexpr char* kBrowseRgdText = "Choose path to Radeon GPU Detective...";
#ifdef Q_OS_WIN32
static const constexpr char* kRgdApplicationName         = "rgd.exe";
static const constexpr char* kRgdApplicationNameInternal = "rgd-Internal.exe";
#else
static const constexpr char* kRgdApplicationName         = "rgd";
static const constexpr char* kRgdApplicationNameInternal = "rgd-Internal";
#endif
static const constexpr char* kRgdExecutablePathKey = "rgd_path";

// Backend test executables
static const constexpr char* kBrowseRgpBackendTestText = "Choose path to RGP Backend Test executable...";
#ifdef Q_OS_WIN32
static const constexpr char* kRgpBackendTestApplicationName         = "RgpBackendTest.exe";
static const constexpr char* kRgpBackendTestApplicationNameInternal = "RgpBackendTest-Internal.exe";
#else
static const constexpr char* kRgpBackendTestApplicationName         = "RgpBackendTest";
static const constexpr char* kRgpBackendTestApplicationNameInternal = "RgpBackendTest-Internal";
#endif
static const constexpr char* kRgpBackendTestExecutablePathKey = "rgp_backend_test_path";

static const constexpr char* kBrowseRmvBackendTestText = "Choose path to RMV Backend Test executable...";
#ifdef Q_OS_WIN32
static const constexpr char* kRmvBackendTestApplicationName         = "RmvBackendTest.exe";
static const constexpr char* kRmvBackendTestApplicationNameInternal = "RmvBackendTest-Internal.exe";
#else
static const constexpr char* kRmvBackendTestApplicationName         = "RmvBackendTest";
static const constexpr char* kRmvBackendTestApplicationNameInternal = "RmvBackendTest-Internal";
#endif
static const constexpr char* kRmvBackendTestExecutablePathKey = "rmv_backend_test_path";

static const constexpr char* kBrowseRraBackendTestText = "Choose path to RRA Backend Test executable...";
#ifdef Q_OS_WIN32
static const constexpr char* kRraBackendTestApplicationName         = "RraBackendTest.exe";
static const constexpr char* kRraBackendTestApplicationNameInternal = "RraBackendTest-Internal.exe";
#else
static const constexpr char* kRraBackendTestApplicationName         = "RraBackendTest";
static const constexpr char* kRraBackendTestApplicationNameInternal = "RraBackendTest-Internal";
#endif
static const constexpr char* kRraBackendTestExecutablePathKey = "rra_backend_test_path";

static const constexpr char* kSettingsFilePathKey        = "settings_file_path";
static const constexpr char* kBrowseSettingsFileText     = "Choose root folder for settings file...";
static const constexpr char* kBrowseTxtEditorText        = "Choose path to preferred text editor...";
static const constexpr char* kTxtEditorExecutablePathKey = "txt_editor_path";

static constexpr char const* kAutoOpenTracesPathKey = "auto_open_traces";

/// @brief The number of milliseconds that must elapse before the file paths are re-validated.
static constexpr int kValidationTime = 200;

namespace rdp
{
    namespace
    {
        inline bool ShouldShowBackendTestUi()
        {
            return qEnvironmentVariableIsSet("RDP_ENABLE_RDF_BACKEND_TEST");
        }
    }  // namespace

    SettingsWidget::SettingsWidget(QWidget* parent)
        : QWidget(parent)
        , ui_(new Ui::SettingsWidget)
    {
        ui_->setupUi(this);

        ui_->color_theme_combo_->setCurrentIndex(ColorThemeType::kColorThemeTypeCount);

        connect(ui_->browse_rgp_btn, &QPushButton::clicked, this, &SettingsWidget::OnBrowseToRgp);
        connect(ui_->rgp_executable_path, &QLineEdit::textChanged, this, &SettingsWidget::OnRgpExecutablePathTextChanged);

        connect(ui_->browse_rmv_btn, &QPushButton::clicked, this, &SettingsWidget::OnBrowseToRmv);
        connect(ui_->rmv_executable_path, &QLineEdit::textChanged, this, &SettingsWidget::OnRmvExecutablePathTextChanged);

        connect(ui_->browse_rra_btn, &QPushButton::clicked, this, &SettingsWidget::OnBrowseToRra);
        connect(ui_->rra_executable_path, &QLineEdit::textChanged, this, &SettingsWidget::OnRraExecutablePathTextChanged);

        connect(ui_->browse_rgd_btn, &QPushButton::clicked, this, &SettingsWidget::OnBrowseToRgd);
        connect(ui_->rgd_executable_path, &QLineEdit::textChanged, this, &SettingsWidget::OnRgdExecutablePathTextChanged);

        // Backend test connections
        if (ui_->browse_rgp_backend_test_btn != nullptr)
        {
            connect(ui_->browse_rgp_backend_test_btn, &QPushButton::clicked, this, &SettingsWidget::OnBrowseToRgpBackendTest);
            connect(ui_->rgp_backend_test_executable_path, &QLineEdit::textChanged, this, &SettingsWidget::OnRgpBackendTestExecutablePathTextChanged);
        }

        if (ui_->browse_rmv_backend_test_btn != nullptr)
        {
            connect(ui_->browse_rmv_backend_test_btn, &QPushButton::clicked, this, &SettingsWidget::OnBrowseToRmvBackendTest);
            connect(ui_->rmv_backend_test_executable_path, &QLineEdit::textChanged, this, &SettingsWidget::OnRmvBackendTestExecutablePathTextChanged);
        }

        if (ui_->browse_rra_backend_test_btn != nullptr)
        {
            connect(ui_->browse_rra_backend_test_btn, &QPushButton::clicked, this, &SettingsWidget::OnBrowseToRraBackendTest);
            connect(ui_->rra_backend_test_executable_path, &QLineEdit::textChanged, this, &SettingsWidget::OnRraBackendTestExecutablePathTextChanged);
        }

        connect(ui_->browse_settings_file_btn, &QPushButton::clicked, this, &SettingsWidget::OnBrowseToSettingsFile);
        connect(ui_->settings_file_path, &QLineEdit::textChanged, this, &SettingsWidget::OnSettingsFilePathTextChanged);
        connect(ui_->browse_rdts, &QPushButton::clicked, this, &SettingsWidget::BrowseToRdts);
        connect(ui_->browse_text_editor_btn, &QPushButton::clicked, this, &SettingsWidget::OnBrowseToTxtEditor);
        connect(ui_->text_editor_executable_path, &QLineEdit::textChanged, this, &SettingsWidget::OnTxtEditorExecutablePathTextChanged);
        connect(ui_->auto_open_toggle, &QCheckBox::stateChanged, this, &SettingsWidget::OnAutoOpenTraceToggled);
        connect(ui_->color_theme_combo_, &QComboBox::currentIndexChanged, this, &SettingsWidget::OnThemeCurrentIndexChanged);

        QPushButton* restore_defaults = ui_->button_box_->button(QDialogButtonBox::StandardButton::RestoreDefaults);
        connect(restore_defaults, &QPushButton::clicked, this, &SettingsWidget::OnRestoreDefaultsClicked);

#ifndef DRIVER_SETTINGS_ENABLED
        ui_->settings_file_path_widget->hide();
#endif
    }

    SettingsWidget::~SettingsWidget() = default;

    void SettingsWidget::Initialize(std::shared_ptr<SettingsManager> settings_manager)
    {
        const QString default_rgp_exe_path          = QueryDefaultPath(kRgpApplicationName, kRgpApplicationNameInternal);
        const QString default_rmv_exe_path          = QueryDefaultPath(kRmvApplicationName, kRmvApplicationNameInternal);
        const QString default_rra_exe_path          = QueryDefaultPath(kRraApplicationName, kRraApplicationNameInternal);
        const QString default_rgd_exe_path          = QueryDefaultPath(kRgdApplicationName, kRgdApplicationNameInternal);
        const QString default_rgp_backend_test_path = QueryDefaultPath(kRgpBackendTestApplicationName, kRgpBackendTestApplicationNameInternal);
        const QString default_rmv_backend_test_path = QueryDefaultPath(kRmvBackendTestApplicationName, kRmvBackendTestApplicationNameInternal);
        const QString default_rra_backend_test_path = QueryDefaultPath(kRraBackendTestApplicationName, kRraBackendTestApplicationNameInternal);
        const QString default_settings_file_path    = "";
        const QString default_txt_editor_exe_path   = GetDefaultTextEditorPath();

        settings_manager_   = std::move(settings_manager);
        QSettings* settings = settings_manager_->GetSettings();
        Q_ASSERT(settings != nullptr);
        if (settings != nullptr)
        {
            // Write initial settings value for RGP path if not available
            if (settings->value(kRgpExecutablePathKey).isNull())
            {
                settings->setValue(kRgpExecutablePathKey, default_rgp_exe_path);
            }

            // Write initial settings value for RMV path if not available
            if (settings->value(kRmvExecutablePathKey).isNull())
            {
                settings->setValue(kRmvExecutablePathKey, default_rmv_exe_path);
            }

            // Write initial settings value for RRA path if not available
            if (settings->value(kRraExecutablePathKey).isNull())
            {
                settings->setValue(kRraExecutablePathKey, default_rra_exe_path);
            }

            // Write initial settings value for RGD path if not available
            if (settings->value(kRgdExecutablePathKey).isNull())
            {
                settings->setValue(kRgdExecutablePathKey, default_rgd_exe_path);
            }

            // Write initial settings value for RGPBackendTest path if not available
            if (settings->value(kRgpBackendTestExecutablePathKey).isNull())
            {
                settings->setValue(kRgpBackendTestExecutablePathKey, default_rgp_backend_test_path);
            }

            // Write initial settings value for RMVBackendTest path if not available
            if (settings->value(kRmvBackendTestExecutablePathKey).isNull())
            {
                settings->setValue(kRmvBackendTestExecutablePathKey, default_rmv_backend_test_path);
            }

            // Write initial settings value for RRABackendTest path if not available
            if (settings->value(kRraBackendTestExecutablePathKey).isNull())
            {
                settings->setValue(kRraBackendTestExecutablePathKey, default_rra_backend_test_path);
            }

            char* env_var = getenv("AMD_CONFIG_DIR");
            if (env_var != nullptr)
            {
                settings->setValue(kSettingsFilePathKey, QString::fromUtf8(env_var));
            }
            else if (settings->value(kSettingsFilePathKey).isNull())
            {
                settings->setValue(kSettingsFilePathKey, default_settings_file_path);
            }

            // Write initial settings value for text editor path if not available
            if (settings->value(kTxtEditorExecutablePathKey).isNull())
            {
                settings->setValue(kTxtEditorExecutablePathKey, default_txt_editor_exe_path);
            }

            if (settings->value(kAutoOpenTracesPathKey).isNull())
            {
                settings->setValue(kAutoOpenTracesPathKey, false);
            }

            if (settings->value(kApplicationSettingsThemeKey).isNull())
            {
                settings->setValue(kApplicationSettingsThemeKey, ColorThemeType::kColorThemeTypeCount);
            }

            settings->sync();

            // Load saved settings
            const QString rgp_path              = settings->value(kRgpExecutablePathKey).toString();
            const QString rmv_path              = settings->value(kRmvExecutablePathKey).toString();
            const QString rra_path              = settings->value(kRraExecutablePathKey).toString();
            const QString rgd_path              = settings->value(kRgdExecutablePathKey).toString();
            const QString rgp_backend_test_path = settings->value(kRgpBackendTestExecutablePathKey).toString();
            const QString rmv_backend_test_path = settings->value(kRmvBackendTestExecutablePathKey).toString();
            const QString rra_backend_test_path = settings->value(kRraBackendTestExecutablePathKey).toString();
            const QString settings_file_path    = settings->value(kSettingsFilePathKey).toString();
            const QString txt_editor_path       = settings->value(kTxtEditorExecutablePathKey).toString();
            const int     color_theme           = settings->value(kApplicationSettingsThemeKey).toInt();
            settings_manager_->ApplyColorThemeOption(static_cast<ColorThemeOption>(color_theme));

            ui_->rgp_executable_path->setText(QDir::toNativeSeparators(rgp_path));
            ui_->rmv_executable_path->setText(QDir::toNativeSeparators(rmv_path));
            ui_->rra_executable_path->setText(QDir::toNativeSeparators(rra_path));
            ui_->rgd_executable_path->setText(QDir::toNativeSeparators(rgd_path));
            ui_->settings_file_path->setText(QDir::toNativeSeparators(settings_file_path));
            ui_->text_editor_executable_path->setText(QDir::toNativeSeparators(txt_editor_path));
            ui_->color_theme_combo_->setCurrentIndex(color_theme);

            if (ui_->rgp_backend_test_executable_path != nullptr)
            {
                ui_->rgp_backend_test_executable_path->setText(QDir::toNativeSeparators(rgp_backend_test_path));
            }
            if (ui_->rmv_backend_test_executable_path != nullptr)
            {
                ui_->rmv_backend_test_executable_path->setText(QDir::toNativeSeparators(rmv_backend_test_path));
            }
            if (ui_->rra_backend_test_executable_path != nullptr)
            {
                ui_->rra_backend_test_executable_path->setText(QDir::toNativeSeparators(rra_backend_test_path));
            }

            UpdateBackendTestVisibility();

            connect(&validation_timer_, &QTimer::timeout, this, &SettingsWidget::ValidatePaths);
            validation_timer_.start(kValidationTime);
            ui_->auto_open_toggle->setChecked(settings->value(kAutoOpenTracesPathKey, false).toBool());
            ValidatePaths();
        }
    }

    void SettingsWidget::ValidatePaths()
    {
        ValidateRgpPath();
        ValidateRmvPath();
        ValidateRraPath();
        ValidateRgdPath();
        if (ui_->rgp_backend_test_executable_path)
            ValidateRgpBackendTestPath();
        if (ui_->rmv_backend_test_executable_path)
            ValidateRmvBackendTestPath();
        if (ui_->rra_backend_test_executable_path)
            ValidateRraBackendTestPath();
        ValidateTxtEditorPath();
#ifdef DRIVER_SETTINGS_ENABLED
        ValidateSettingsFilePath();
#endif
    }

    QString SettingsWidget::QueryDefaultPath(const QString& application_name, const QString& application_internal_name)
    {
        // Need to first check for the existence of a public or internal version
        // of RGP
        QString   public_path   = QDir::toNativeSeparators(QString(".") + QDir::separator() + application_name);
        QString   internal_path = QDir::toNativeSeparators(QString(".") + QDir::separator() + application_internal_name);
        QFileInfo public_info(public_path);
        QFileInfo internal_info(internal_path);

        // If the public version of RMV executable exists in path
        if (public_info.exists())
        {
            return public_path;
        }

        if (internal_info.exists())
        {
            return internal_path;
        }

        // If neither exist, just default to the public path
        return public_path;
    }

    QString SettingsWidget::GetDefaultTextEditorPath()
    {
#ifdef Q_OS_WINDOWS
        return "C:\\Windows\\notepad.exe";
#else
        return "/usr/bin/gnome-text-editor";
#endif
    }

    void SettingsWidget::OnColorThemeSelected(ColorThemeOption color_theme_option)
    {
        const ColorThemeOption current_theme = settings_manager_->GetColorThemeOption();
        if (current_theme == color_theme_option)
        {
            return;
        }

        // Save the selected theme index
        QSettings* settings = settings_manager_->GetSettings();
        if (settings != nullptr)
        {
            settings->setValue(kApplicationSettingsThemeKey, static_cast<int>(color_theme_option));
            settings->sync();
        }

        if (current_theme == ColorThemeOption::kSystem || color_theme_option == ColorThemeOption::kSystem)
        {
            const ColorThemeOption os_type = static_cast<ColorThemeOption>(QtCommon::QtUtils::DetectOsSetting());
            if (os_type == color_theme_option || current_theme == os_type)
            {
                return;
            }
        }

        settings_manager_->ApplyColorThemeOption(color_theme_option);
    }

    void SettingsWidget::OnRestoreDefaultsClicked(bool clicked)
    {
        Q_UNUSED(clicked)

        if (ui_->rgp_executable_path)
            ui_->rgp_executable_path->setText(QueryDefaultPath(kRgpApplicationName, kRgpApplicationNameInternal));
        if (ui_->rmv_executable_path)
            ui_->rmv_executable_path->setText(QueryDefaultPath(kRmvApplicationName, kRmvApplicationNameInternal));
        if (ui_->rra_executable_path)
            ui_->rra_executable_path->setText(QueryDefaultPath(kRraApplicationName, kRraApplicationNameInternal));
        if (ui_->rgd_executable_path)
            ui_->rgd_executable_path->setText(QueryDefaultPath(kRgdApplicationName, kRgdApplicationNameInternal));
        if (ui_->rgp_backend_test_executable_path)
            ui_->rgp_backend_test_executable_path->setText(QueryDefaultPath(kRgpBackendTestApplicationName, kRgpBackendTestApplicationNameInternal));
        if (ui_->rmv_backend_test_executable_path)
            ui_->rmv_backend_test_executable_path->setText(QueryDefaultPath(kRmvBackendTestApplicationName, kRmvBackendTestApplicationNameInternal));
        if (ui_->rra_backend_test_executable_path)
            ui_->rra_backend_test_executable_path->setText(QueryDefaultPath(kRraBackendTestApplicationName, kRraBackendTestApplicationNameInternal));
        if (ui_->text_editor_executable_path)
            ui_->text_editor_executable_path->setText(GetDefaultTextEditorPath());
        if (ui_->auto_open_toggle)
            ui_->auto_open_toggle->setChecked(false);
        if (ui_->color_theme_combo_)
            ui_->color_theme_combo_->setCurrentIndex(kColorThemeTypeCount);
    }

    void SettingsWidget::OnBrowseToRgp()
    {
        OnBrowseToExe(ui_->rgp_executable_path, kBrowseRgpText);
    }

    void SettingsWidget::OnBrowseToRmv()
    {
        OnBrowseToExe(ui_->rmv_executable_path, kBrowseRmvText);
    }

    void SettingsWidget::OnBrowseToRra()
    {
        OnBrowseToExe(ui_->rra_executable_path, kBrowseRraText);
    }

    void SettingsWidget::OnBrowseToRgd()
    {
        OnBrowseToExe(ui_->rgd_executable_path, kBrowseRgdText);
    }

    void SettingsWidget::OnBrowseToRgpBackendTest()
    {
        OnBrowseToExe(ui_->rgp_backend_test_executable_path, kBrowseRgpBackendTestText);
    }

    void SettingsWidget::OnBrowseToRmvBackendTest()
    {
        OnBrowseToExe(ui_->rmv_backend_test_executable_path, kBrowseRmvBackendTestText);
    }

    void SettingsWidget::OnBrowseToRraBackendTest()
    {
        OnBrowseToExe(ui_->rra_backend_test_executable_path, kBrowseRraBackendTestText);
    }

    void SettingsWidget::OnBrowseToSettingsFile()
    {
        const QString& current_path = ui_->settings_file_path->text();
        const QString& new_path     = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(this, kBrowseSettingsFileText));
        if (!new_path.isEmpty() && current_path.compare(new_path) != 0)
        {
            ui_->settings_file_path->setText(new_path);
        }
    }

    void SettingsWidget::OnBrowseToTxtEditor()
    {
        OnBrowseToExe(ui_->text_editor_executable_path, kBrowseTxtEditorText);
    }

    void SettingsWidget::OnBrowseToExe(QLineEdit* line_edit, const QString& browse_text)
    {
        const QString& current_path = line_edit->text();
        const QString& new_path     = QDir::toNativeSeparators(QFileDialog::getOpenFileName(this, browse_text, current_path));
        if (!new_path.isEmpty() && current_path.compare(new_path) != 0)
        {
            line_edit->setText(new_path);
        }
    }

    void SettingsWidget::BrowseToRdts()
    {
        const QString& new_path = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(this, "Locate Radeon Developer Tool Suite"));
        if (new_path.isEmpty())
        {
            return;
        }

        const QDir rdts_dir(new_path);
        if (rdts_dir.isEmpty())
        {
            return;
        }

        if (ui_->rgp_executable_path)
            ui_->rgp_executable_path->setText(rdts_dir.absoluteFilePath(kRgpApplicationName));
        if (ui_->rmv_executable_path)
            ui_->rmv_executable_path->setText(rdts_dir.absoluteFilePath(kRmvApplicationName));
        if (ui_->rra_executable_path)
            ui_->rra_executable_path->setText(rdts_dir.absoluteFilePath(kRraApplicationName));
        if (ui_->rgd_executable_path)
            ui_->rgd_executable_path->setText(rdts_dir.absoluteFilePath(kRgdApplicationName));
        if (ui_->rgp_backend_test_executable_path)
            ui_->rgp_backend_test_executable_path->setText(rdts_dir.absoluteFilePath(kRgpBackendTestApplicationName));
        if (ui_->rmv_backend_test_executable_path)
            ui_->rmv_backend_test_executable_path->setText(rdts_dir.absoluteFilePath(kRmvBackendTestApplicationName));
        if (ui_->rra_backend_test_executable_path)
            ui_->rra_backend_test_executable_path->setText(rdts_dir.absoluteFilePath(kRraBackendTestApplicationName));
    }

    void SettingsWidget::OnRgpExecutablePathTextChanged(const QString& text)
    {
        OnExecutablePathChanged(text, kRgpExecutablePathKey);
    }

    void SettingsWidget::OnRmvExecutablePathTextChanged(const QString& text)
    {
        OnExecutablePathChanged(text, kRmvExecutablePathKey);
    }

    void SettingsWidget::OnRraExecutablePathTextChanged(const QString& text)
    {
        OnExecutablePathChanged(text, kRraExecutablePathKey);
    }

    void SettingsWidget::OnRgdExecutablePathTextChanged(const QString& text)
    {
        OnExecutablePathChanged(text, kRgdExecutablePathKey);
    }

    void SettingsWidget::OnRgpBackendTestExecutablePathTextChanged(const QString& text)
    {
        OnExecutablePathChanged(text, kRgpBackendTestExecutablePathKey);
    }

    void SettingsWidget::OnRmvBackendTestExecutablePathTextChanged(const QString& text)
    {
        OnExecutablePathChanged(text, kRmvBackendTestExecutablePathKey);
    }

    void SettingsWidget::OnRraBackendTestExecutablePathTextChanged(const QString& text)
    {
        OnExecutablePathChanged(text, kRraBackendTestExecutablePathKey);
    }

    void SettingsWidget::OnSettingsFilePathTextChanged(const QString& text)
    {
        OnExecutablePathChanged(text, kSettingsFilePathKey);
    }

    void SettingsWidget::OnTxtEditorExecutablePathTextChanged(const QString& text)
    {
        OnExecutablePathChanged(text, kTxtEditorExecutablePathKey);
    }

    void SettingsWidget::OnExecutablePathChanged(const QString& text, const QString& key)
    {
        QSettings* settings = settings_manager_->GetSettings();
        if (settings != nullptr)
        {
            settings->setValue(key, text);
            settings->sync();
        }
    }

    void SettingsWidget::OnAutoOpenTraceToggled(int state)
    {
        QSettings* settings = settings_manager_->GetSettings();
        if (settings != nullptr)
        {
            settings->setValue(kAutoOpenTracesPathKey, state == Qt::Checked);
            settings->sync();
        }
    }

    void SettingsWidget::OnThemeCurrentIndexChanged(int index)
    {
        if (index == static_cast<int>(settings_manager_->GetColorThemeOption()))
        {
            return;
        }

        const ColorThemeOption color_theme = static_cast<ColorThemeOption>(index);
        OnColorThemeSelected(color_theme);
    }

    bool SettingsWidget::ValidateRgpPath()
    {
        return ValidatePath(ui_->rgp_executable_path, ui_->profilerPathStatusLabel, "Invalid RGP Executable");
    }

    bool SettingsWidget::ValidateRmvPath()
    {
        return ValidatePath(ui_->rmv_executable_path, ui_->rmvPathStatusLabel, "Invalid RMV Executable");
    }

    bool SettingsWidget::ValidateRraPath()
    {
        return ValidatePath(ui_->rra_executable_path, ui_->rraPathStatusLabel, "Invalid RRA Executable");
    }

    bool SettingsWidget::ValidateRgdPath()
    {
        return ValidatePath(ui_->rgd_executable_path, ui_->rgdPathStatusLabel, "Invalid RGD Executable");
    }
    bool SettingsWidget::ValidateRgpBackendTestPath()
    {
        return ui_->rgp_backend_test_executable_path
                   ? ValidatePath(ui_->rgp_backend_test_executable_path, ui_->rgpBackendTestPathStatusLabel, "Invalid RGP Backend Test Executable")
                   : false;
    }
    bool SettingsWidget::ValidateRmvBackendTestPath()
    {
        return ui_->rmv_backend_test_executable_path
                   ? ValidatePath(ui_->rmv_backend_test_executable_path, ui_->rmvBackendTestPathStatusLabel, "Invalid RMV Backend Test Executable")
                   : false;
    }
    bool SettingsWidget::ValidateRraBackendTestPath()
    {
        return ui_->rra_backend_test_executable_path
                   ? ValidatePath(ui_->rra_backend_test_executable_path, ui_->rraBackendTestPathStatusLabel, "Invalid RRA Backend Test Executable")
                   : false;
    }

    bool SettingsWidget::ValidateSettingsFilePath()
    {
        const QString path = ui_->settings_file_path->text();

        QFileInfo file_info(path);

        if (!file_info.exists() || !file_info.isDir())
        {
            ui_->settingsFilePathStatusLabel->setText("Invalid settings file path");
            ui_->settingsFilePathStatusLabel->show();
            return false;
        }
        ui_->settingsFilePathStatusLabel->hide();
        return true;
    }

    bool SettingsWidget::ValidateTxtEditorPath()
    {
        return ValidatePath(ui_->text_editor_executable_path, ui_->textEditorPathStatusLabel, "Invalid text editor");
    }

    bool SettingsWidget::ValidatePath(QLineEdit* line_edit, QLabel* status_label, const QString& invalid_text)
    {
        const QString path = line_edit->text();

        QFileInfo file_info(path);
        if (!file_info.exists() || !file_info.isExecutable() || file_info.isDir())
        {
            status_label->setText(invalid_text);
            status_label->show();
            return false;
        }

        status_label->hide();
        return true;
    }

    void SettingsWidget::UpdateBackendTestVisibility()
    {
        const bool show = ShouldShowBackendTestUi();
        if (ui_->backend_test_group_box)
            ui_->backend_test_group_box->setVisible(show);
        if (ui_->backend_tests_separator_line)
            ui_->backend_tests_separator_line->setVisible(show);  // Adjust spacer size instead of setVisible (QSpacerItem has no setVisible)
        if (ui_->backend_tests_separator_spacer)
        {
            ui_->backend_tests_separator_spacer->changeSize(show ? 20 : 0, show ? 12 : 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
            if (ui_->verticalLayout)
                ui_->verticalLayout->invalidate();
        }
    }

}  // namespace rdp
