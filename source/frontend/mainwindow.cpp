// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP main window class implementation

#include "mainwindow.h"

#include <algorithm>
#include <memory>
#include <sstream>

#ifdef Q_OS_LINUX
#include <sys/stat.h>
#endif

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStyleHints>
#include <QTabWidget>
#include <QToolButton>
#include <QVariant>
#include <QWindow>

#include <qt_common/custom_widgets/navigation_list_model.h>
#include <qt_common/utils/qt_util.h>

#include "models/module_model.h"
#include "models/preset_model.h"

#include "application_context_menu.h"
#include "bottom_bar_tab_widget.h"
#include "collapsible_pane.h"
#include "connection_widget.h"
#include "definitions.h"
#include "logging/debug_log_widget.h"
#include "logging/logging_manager.h"
#include "system_info_widget.h"
#include "system_tab_widget.h"
#include "timeout_dialog.h"
#include "ui_mainwindow.h"
#include "version.h"

#include "ddAmdGpuInfo.h"

static constexpr const char* kMainWindowObjectName   = "RDPMainWindow";
static constexpr const char* kRedIndicatorIconPath   = ":/red_light.svg";
static constexpr const char* kGreenIndicatorIconPath = ":/green_light.svg";
static constexpr const char* kHelpIconPath           = ":/question.svg";
static constexpr const char* kGearIconPath           = ":/gear.svg";
static constexpr const char* kBugIconPath            = ":/bug.svg";
static const int             kNavTabMarginLeft       = 10;
static const int             kNavTabMarginRight      = 10;
static const int             kNavTabMarginTop        = 10;
static const int             kNavTabMarginBottom     = 10;

// Help file locations
static constexpr const char* kHelpFilePath = "/help/rdp/index.html";

namespace rdp
{
    enum class MainWindowTabType : uint32_t
    {
        kConnection          = 0,
        kCapture             = 1,
        kSystem              = 2,
        kSpacer              = 3,
        kDriverSettingsDummy = 4,
        kSettings            = 5
    };

    MainWindow::MainWindow(const std::shared_ptr<ModuleModel>&     module_model,
                           const std::shared_ptr<TimeoutModel>&    timeout_model,
                           const std::shared_ptr<ConnectionModel>& connection_model,
                           const std::shared_ptr<SystemInfoModel>& system_info_model,
                           const std::shared_ptr<SettingsManager>& settings_manager,
                           QWidget*                                parent)
        : QMainWindow(parent)
        , ui_(new Ui::MainWindow)
        , settings_manager_(std::move(settings_manager))
        , module_model_(std::move(module_model))
        , preset_model_(new PresetModel(module_model_))
        , connection_model_(std::move(connection_model))
        , api_model_(new ApiModel(system_info_model))
        , application_model_(new ApplicationModel(settings_manager_, connection_model_, module_model_, api_model_))
        , blocklist_model_(new BlocklistModel(connection_model_, settings_manager_))
        , system_info_model_(std::move(system_info_model))
        , green_icon_(kGreenIndicatorIconPath)
        , red_icon_(kRedIndicatorIconPath)
        , help_icon_(kHelpIconPath)
        , gear_icon_(kGearIconPath)
        , bug_icon_(kBugIconPath)

    {
        setObjectName(kMainWindowObjectName);
        setWindowTitle(RDP_TITLE);

        bug_report_generator_ = std::make_unique<BugReportGenerator>(system_info_model_, module_model_);

        ui_->setupUi(this);
        ui_->message_overlay_container->SetBackground(ui_->main_tab_widget);
        connect(ui_->message_overlay_container, &MessageOverlayContainer::MessageOverlayShown, this, &MainWindow::BringToForeground);

        connect(connection_model_.get(), &ConnectionModel::NetDisconnected, this, &MainWindow::NetDisconnected);
        connect(connection_model_.get(), &ConnectionModel::NetDisconnecting, this, &MainWindow::NetDisconnecting);
        connect(connection_model_.get(), &ConnectionModel::NetConnecting, this, &MainWindow::NetConnecting);
        connect(connection_model_.get(), &ConnectionModel::NetConnected, this, &MainWindow::NetConnected);

        Q_ASSERT(!connection_model_->IsConnected());
        NetDisconnected();  // The connection model starts as disconnected.

        connect(system_info_model_.get(), &SystemInfoModel::EtwStatusCodeNonzero, this, &MainWindow::OnEtwStatusCodeNonzero);
        connect(system_info_model_.get(), &SystemInfoModel::SystemInfoFailedToLoad, this, &MainWindow::OnSystemInfoFailedToLoad);
        connect(system_info_model_.get(), &SystemInfoModel::AddUserToGroupBatNeeded, this, &MainWindow::OnAddUserToGroup);
        connect(system_info_model_.get(), &SystemInfoModel::UnsupportedHandheldDriver, this, &MainWindow::OnUnsupportedHandheldDriver);

        connect(application_model_.get(), &ApplicationModel::ClientConnectedWhenAlreadyConnected, this, &MainWindow::OnClientConnectedWhenAlreadyConnected);

        // Initialize UI with models
        ui_->settings_tab->Initialize(settings_manager_);
        connect(ui_->main_tab_widget, &QTabWidget::currentChanged, this, &MainWindow::TabChanged);

        // Default to connection tab. (Regardless of what is default set in UI file)
        ui_->main_tab_widget->setCurrentIndex(static_cast<uint32_t>(MainWindowTabType::kConnection));

        // Apply content margins to the tab items
        // for the navigation tab bar
        const QList<QTabBar*> tab_bar = ui_->main_tab_widget->findChildren<QTabBar*>();
        foreach (QTabBar* item, tab_bar)
        {
            if (item != nullptr)
            {
                item->setContentsMargins(kNavTabMarginLeft, kNavTabMarginTop, kNavTabMarginRight, kNavTabMarginBottom);
            }
        }

        // Set application tab default splitter section sizes
        ui_->splitter->setStretchFactor(0, 0);
        ui_->splitter->setStretchFactor(1, 1);

        bottom_tab_widget_ = new BottomBarTabWidget(this, system_info_model_);
        connect(ui_->vertical_splitter,
                &BottomBarSplitter::DraggedPastMinSizeInDirection,
                bottom_tab_widget_,
                &BottomBarTabWidget::SplitterDraggedPastMinSizeInDirection);

        ui_->vertical_splitter->insertWidget(1, bottom_tab_widget_);

        QString settings_name = "driver experiments";

        driver_settings_label_ = new QLabel(this);
        driver_settings_label_->setText(QString("<b>Changes have been made to %1</b>").arg(settings_name));
        driver_settings_label_->setStyleSheet(QString("color: %1; font-size: 7pt; padding-right: 10px").arg(QColor(Qt::darkYellow).lighter(140).name()));

        ui_->main_tab_widget->setTabEnabled(static_cast<uint32_t>(MainWindowTabType::kDriverSettingsDummy), false);
        ui_->main_tab_widget->setTabVisible(static_cast<uint32_t>(MainWindowTabType::kDriverSettingsDummy), false);
        ui_->main_tab_widget->SetTabTool(static_cast<uint32_t>(MainWindowTabType::kDriverSettingsDummy), driver_settings_label_, QTabBar::RightSide);

        // Create the bug report button
        auto* bug_report_button = new QPushButton("");
        bug_report_button->setIcon(bug_icon_);
        bug_report_button->setObjectName("BugReportButton");
        bug_report_button->setCursor(Qt::PointingHandCursor);

        ui_->main_tab_widget->addTab(new QWidget, "");
        ui_->main_tab_widget->SetTabTool(ui_->main_tab_widget->count() - 1, bug_report_button, QTabBar::RightSide);

        connect(bug_report_button, &QPushButton::clicked, bug_report_generator_.get(), &BugReportGenerator::CopyReport);

        // Create help button
        auto* help_button = new QPushButton();
        help_button->setIcon(help_icon_);
        help_button->setObjectName("HelpButton");
        help_button->setCursor(Qt::PointingHandCursor);

        // Create a tab for the help button
        ui_->main_tab_widget->setTabIcon(0, red_icon_);
        ui_->main_tab_widget->addTab(new QWidget, "");
        ui_->main_tab_widget->SetTabTool(ui_->main_tab_widget->count() - 1, help_button, QTabBar::RightSide);

        connect(help_button, &QPushButton::clicked, []() {
        // Open help file
#ifdef Q_OS_MACOS
            QDir dir(QCoreApplication::applicationDirPath());
            dir.cdUp();
            dir.cd("Resources");
            const QString help_path = dir.path() + kHelpFilePath;
#else
            const QString help_path = QCoreApplication::applicationDirPath() + kHelpFilePath;
#endif
            const QUrl file_url = QUrl::fromLocalFile(help_path);
            if (!QDesktopServices::openUrl(file_url))
            {
                RDP_LOG_ERROR("Failed to open help at: {}", qUtf8Printable(help_path));
            }
        });

        // Create the settings button
        settings_button_ = new QPushButton();
        settings_button_->setIcon(gear_icon_);
        settings_button_->setObjectName("SettingsButton");
        settings_button_->setCursor(Qt::PointingHandCursor);
        ui_->main_tab_widget->SetTabTool(static_cast<uint32_t>(MainWindowTabType::kSettings), settings_button_, QTabBar::RightSide);

        connect(settings_button_, &QPushButton::clicked, [&]() {
            ui_->main_tab_widget->setCurrentIndex(static_cast<uint32_t>(MainWindowTabType::kSettings));
            settings_button_->setStyleSheet("#SettingsButton { border-color: rgb(224,30,55); }");
        });

        ui_->main_tab_widget->SetSpacerIndex(static_cast<uint32_t>(MainWindowTabType::kSpacer));

        // Create the Connection widget and add it into the MainWindow's layout.
        connection_widget_ = new ConnectionWidget(settings_manager_, connection_model_, std::make_shared<TimeoutDialog>(timeout_model));
        ui_->connection_tab_grid->addWidget(connection_widget_);

        // Load geometry settings for window
        QSettings* settings = settings_manager_->GetSettings();
        Q_ASSERT(settings != nullptr);

        restoreGeometry(settings->value("mainwindow/geometry").toByteArray());
        restoreState(settings->value("mainwindow/state").toByteArray());

        const QVariant bottom_bar_collapsed = settings->value("mainwindow/vsplitter/collapsed");
        bottom_tab_widget_->SetCollapsed(bottom_bar_collapsed.isNull() || bottom_bar_collapsed.toBool(), true);

        InitializeModels();

        // Restore saved navigation tab and system sub-tab indices.
        if (settings->contains("mainwindow/tab_index"))
        {
            const int tab_index = settings->value("mainwindow/tab_index").toInt();
            if (tab_index >= 0 && tab_index < ui_->main_tab_widget->count())
            {
                ui_->main_tab_widget->setCurrentIndex(tab_index);
            }
        }

        if (system_tab_widget_ != nullptr && settings->contains("mainwindow/system_tab_index"))
        {
            const int system_tab_index = settings->value("mainwindow/system_tab_index").toInt();
            if (system_tab_index >= 0 && system_tab_index < system_tab_widget_->count())
            {
                system_tab_widget_->setCurrentIndex(system_tab_index);
            }
        }
    }

    void MainWindow::InitializeModels()
    {
        connect(system_info_model_.get(), &SystemInfoModel::Loaded, blocklist_model_.get(), &BlocklistModel::OnSystemInfoModelLoaded);

        api_model_->Load();

        api_proxy_model_ = std::make_shared<ApiProxyModel>(module_model_);
        api_proxy_model_->SetSourceApiModel(api_model_);

        connect(blocklist_model_.get(), &BlocklistModel::Loaded, application_model_.get(), &ApplicationModel::OnBlocklistModelLoaded);
        blocklist_model_->Load();

        connect(application_model_.get(), &ApplicationModel::Loaded, connection_widget_, &ConnectionWidget::OnApplicationModelLoaded);
        application_model_->Load();
        preset_model_->Load();

        ui_->module_widget->SetModels(module_model_, preset_model_);
        ui_->sidebar->SetModels(api_proxy_model_, application_model_, blocklist_model_, connection_model_, module_model_, preset_model_);

        // Create the system tab widget and add it to the SYSTEM tab layout.
        system_tab_widget_ = new SystemTabWidget();
        ui_->system_tab_layout->addWidget(system_tab_widget_);
        system_tab_widget_->SetModels(module_model_);

        connection_model_->OnBind();
    }

    MainWindow::~MainWindow()
    {
    }

    void MainWindow::OnEtwStatusCodeNonzero(bool has_permission)
    {
        const QString permission_message = has_permission ? QString() : kEtwNoPermissionText;
        MessageOverlay::WarningAsync(kEtwWarningTitle, QString(kEtwWarningText).arg(permission_message));
    }

    void MainWindow::OnUnsupportedHandheldDriver()
    {
        MessageOverlay::WarningAsync("Driver compatibility issue detected",
                                     "The installed driver is either unsupported or may be incorrectly detected due to missing version information.\nPlease "
                                     "verify the driver version and consider updating to a supported version for optimal performance.");
    }

    void MainWindow::OnAddUserToGroup()
    {
        if (connection_model_->GetActiveConnectionType() == ConnectionType::kLocal)
        {
#ifdef Q_OS_WINDOWS
            MessageOverlay::WarningAsync(
                "Sync Primitives support disabled",
                "Captured RGP profiles for applications using DirectX 12 will not contain any Signal and Wait data.\n"
                "Run the AddUserToGroup.bat batch file as Administrator?",
                "",
                [](QDialogButtonBox::StandardButton result) {
                    const QString script_path = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/scripts");
                    if (!QFile::exists(script_path) || result & QDialogButtonBox::StandardButton::No)
                    {
                        return;
                    }

                    const std::wstring working_directory     = std::filesystem::path(script_path.toStdString()).wstring();
                    constexpr int      safe_result_threshold = 32;  // See documentation for ShellExecute()
                    INT_PTR instance = (INT_PTR)ShellExecuteW(NULL, L"runas", L"AddUserToGroup.bat", nullptr, working_directory.c_str(), SW_SHOWNORMAL);

                    if (instance < safe_result_threshold)
                    {
                        DWORD status = GetLastError();
                        if (status != ERROR_CANCELLED)
                        {
                            LPSTR  buffer = nullptr;
                            size_t size   = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                                         NULL,
                                                         status,
                                                         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                                         (LPSTR)&buffer,
                                                         0,
                                                         NULL);
                            Q_UNUSED(size);

                            RDP_LOG_ERROR("Failed to execute AddUserToGroup.bat: {}", buffer);
                            LocalFree(buffer);
                        }
                    }
                },
                QDialogButtonBox::Yes | QDialogButtonBox::No);
#endif
        }
        else
        {
            MessageOverlay::WarningAsync("Sync Primitives support disabled",
                                         "Captured profiles will not contain any Signal and Wait data.\n"
                                         "To resolve this issue, please run the AddUserToGroup.bat batch file as Administrator on the remote system.\n"
                                         "Please see the Known Issues section of the documentation for more information.");
        }
    }

    void MainWindow::NetDisconnecting()
    {
        setWindowTitle(QString("Disconnecting - %1").arg(RDP_TITLE));
    }

    void MainWindow::NetDisconnected()
    {
        if (shutdown_state_ == ShutdownState::kRequested)
        {
            shutdown_state_ = ShutdownState::kReady;

            QApplication::closeAllWindows();
        }

        setWindowTitle(QString("No connection - %1").arg(RDP_TITLE));
        ui_->main_tab_widget->setTabIcon(0, red_icon_);
    }

    void MainWindow::NetConnecting()
    {
        setWindowTitle(QString("Connecting - %1").arg(RDP_TITLE));
    }

    void MainWindow::NetConnected(const QString& description, bool was_reconnection)
    {
        setWindowTitle(QString("%1 - %2").arg(description, RDP_TITLE));

        if (!was_reconnection)
        {
            // Only auto-switch to Capture on new connection if the user is still on the Connection tab.
            // If they restored a different tab from saved state, respect that choice.
            if (ui_->main_tab_widget->currentIndex() == static_cast<int>(MainWindowTabType::kConnection))
            {
                ui_->main_tab_widget->setCurrentIndex(static_cast<uint32_t>(MainWindowTabType::kCapture));
            }
        }

        ui_->main_tab_widget->setTabIcon(0, green_icon_);
    }

    void MainWindow::BringToForeground()
    {
#ifdef Q_OS_WIN
        SetWindowPos((HWND)this->winId(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        Qt::WindowStates state = Qt::WindowActive;
        if (isMaximized() == true)
        {
            state |= Qt::WindowMaximized;
        }
        setWindowState(state);
        raise();

        if (isMinimized() == true)
        {
            showNormal();
        }

        show();
        SetWindowPos((HWND)this->winId(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
#else
        raise();
        if (!isMaximized())
        {
            showNormal();
        }
        // Note: On Linux, setWindowState(Qt::WindowActive) fails to restore window and also prevents raise() from bring the window to the foreground.
#endif
    }

    void MainWindow::closeEvent(QCloseEvent* event)
    {
        if (shutdown_state_ == ShutdownState::kReady)
        {
            QApplication::closeAllWindows();
            event->accept();

            return;
        }

        ui_->message_overlay_container->Close();

        shutdown_state_ = ShutdownState::kRequested;

        if (QSettings* settings = settings_manager_->GetSettings(); settings != nullptr)
        {
            settings->setValue("mainwindow/geometry", saveGeometry());
            settings->setValue("mainwindow/state", saveState());
            settings->setValue("mainwindow/vsplitter/collapsed", bottom_tab_widget_->IsCollapsed());
            settings->setValue("mainwindow/tab_index", ui_->main_tab_widget->currentIndex());

            if (system_tab_widget_ != nullptr)
            {
                settings->setValue("mainwindow/system_tab_index", system_tab_widget_->currentIndex());
            }

            settings->sync();
        }

        preset_model_->SavePresets();
        application_model_->Save();

        if (connection_model_->IsConnected())
        {
            connection_model_->DisconnectTool();
            event->ignore();

            return;
        }

        QApplication::closeAllWindows();
        event->accept();
    }

    void MainWindow::AttemptNewRemoteConnection(const QString& target)
    {
        connection_widget_->AttemptNewRemoteConnection(target);
    }

    void MainWindow::AttemptAutoConnection()
    {
        connection_widget_->AttemptAutoConnection();
    }

    void MainWindow::OnSystemInfoFailedToLoad()
    {
        // We use a message box as opposed to MessageOverlay, since there might already be an alert presented and this alert should supersede it.
        // We also want this to be blocking, so that nothing else happens that could cause RDP to crash (MessageOverlay is async).
        QMessageBox warning_message(QMessageBox::Critical,
                                    "Error",
                                    "The system information unexpectedly failed to load.\n\nWhen reporting this, please include detailed system "
                                    "specifications including CPU, GPU, operating system and driver version.\n\nRadeon Developer Panel will now exit.");

        warning_message.exec();
        QCoreApplication::quit();
    }

    void MainWindow::OnClientConnectedWhenAlreadyConnected(const QString& app_name, qint64 pid)
    {
        MessageOverlay::CriticalAsync(QString("Existing application already connected."),
                                      QString("%1 [PID %2] tried to connect, but running multiple applications is not supported.").arg(app_name).arg(pid),
                                      QString("MultipleApplicationWarning-%1-%2").arg(app_name).arg(pid));
    }

    void MainWindow::TabChanged(int index)
    {
        if (settings_button_ == nullptr)
        {
            return;
        }

        // Remove the highlight off of the settings button
        if (index != static_cast<uint32_t>(MainWindowTabType::kSettings))
        {
            settings_button_->setStyleSheet("");
        }
    }

    // The windows headers add a max() macro, so we use a custom max function here to avoid compilations on Linux
    inline int Max(int a, int b)
    {
        return a > b ? a : b;
    }

    void MainWindow::SetBottomBarHeight(int new_height)
    {
        // In order for the bottom bar to grow at a constant window size, other views need to shrink (but not past their minimum size). If we can't shrink
        // all the views by the needed amount we need to resize the window.
        int height_to_subtract = Max(new_height - bottom_tab_widget_->geometry().height(), 0);

        QList<int> current_sizes    = ui_->vertical_splitter->sizes();
        bool       found_bottom_bar = false;

        for (int child_idx = 0; child_idx < ui_->vertical_splitter->count(); ++child_idx)
        {
            Q_ASSERT(child_idx < current_sizes.size());

            QWidget* widget = ui_->vertical_splitter->widget(child_idx);
            if (widget == bottom_tab_widget_)
            {
                current_sizes[child_idx] = new_height;
                found_bottom_bar         = true;

                continue;
            }

            const int minimum_height   = widget->minimumSizeHint().height();
            const int new_child_height = Max(minimum_height, current_sizes[child_idx] - height_to_subtract);

            height_to_subtract -= current_sizes[child_idx] - new_child_height;
        }

        if (!found_bottom_bar)
        {
            return;
        }

        ui_->vertical_splitter->setSizes(current_sizes);

        if (height_to_subtract != 0)
        {
            const QSize current_size = size();
            resize(current_size.width(), current_size.height() + height_to_subtract);
        }
    }
}  // namespace rdp
