// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  The Main Window interface for the Radeon Developer Service.

#include "main_window.h"
#include "ui_main_window.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QProcess>
#include <QScreen>
#include <QStyle>
#include <QThread>
#include <QWindow>

#include <g_RouterUtilsModuleInterface.h>
#include <g_SiphonModuleInterface.h>
#include <g_SystemTraceModuleStatic.h>

#include "configuration_window.h"
#include "definitions.h"
#include "settings.h"
#include "version.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui_(new Ui::MainWindow)
    , quit_action_(nullptr)
    , configure_action_(nullptr)
    , tray_icon_menu_(nullptr)
    , tray_icon_(nullptr)
    , router_(DD_API_INVALID_HANDLE)
    , router_port_(kDefaultConnectionPort)
    , previous_router_port_(static_cast<uint32_t>(-1))
{
    settings_ = new Settings(this);
    settings_->Load();

    ui_->setupUi(this);

    // Create the configuration window, but don't show it until the user wants to see it.
    configuration_window_ = new ConfigurationWindow(settings_);
    configuration_window_->setWindowIcon(QIcon(kRDSIconName));
    configuration_window_->hide();

    CreateActions();
    CreateTrayIcon();

    InitializeService();

    connect(configuration_window_, &ConfigurationWindow::RouterEndpointUpdated, this, &MainWindow::OnRouterEndpointUpdated);
    connect(this, &MainWindow::RouterPortUpdated, configuration_window_, &ConfigurationWindow::OnRouterPortUpdated);
    connect(this, &MainWindow::ConnectFailed, configuration_window_, &ConfigurationWindow::OnConnectFailed);
    connect(this, &MainWindow::Connected, configuration_window_, &ConfigurationWindow::OnConnectSucceeded);
    connect(this, &MainWindow::TerminateProcess, this, &MainWindow::OnTerminateProcessEmitted);
}

MainWindow::~MainWindow() Q_DECL_NOEXCEPT
{
    ShutdownService();

    ui_.reset();
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
}

void MainWindow::CreateTrayIcon()
{
    tray_icon_menu_ = new QMenu(this);
    //tray_icon_menu_->setStyleSheet("background-color: rgb(255, 255, 255);");
    //tray_icon_menu_->setPalette(settings_->GetCurrentPalette());

    tray_icon_menu_->addAction(configure_action_);
    tray_icon_menu_->addAction(quit_action_);

    tray_icon_ = new QSystemTrayIcon(this);
    tray_icon_->setContextMenu(tray_icon_menu_);

    connect(tray_icon_, &QSystemTrayIcon::activated, this, &MainWindow::OnTrayIconActivated);

    // The tray icon tooltip is used to display the product name and version.
    QString product_tooltip = kProductNameString;
    product_tooltip.append(" - ");
    product_tooltip.append(RDP_VERSION);

    tray_icon_->setToolTip(product_tooltip);
    tray_icon_->setIcon(QIcon(kRDSIconName));
    tray_icon_->show();
}

void MainWindow::CreateActions()
{
    // Action for quitting the application
    quit_action_ = new QAction(kQuitContextMenu, this);
    connect(quit_action_, &QAction::triggered, QApplication::instance(), &QApplication::quit);

    // Action for opening the configuration window
    configure_action_ = new QAction(kConfigureContextMenu, this);
    connect(configure_action_, &QAction::triggered, this, &MainWindow::OnConfigureTriggered);
}

void MainWindow::InitializeService()
{
    QCommandLineParser parser;

    // Add options
    parser.addHelpOption();
    parser.addOption(QCommandLineOption("port", "RDS listen port", "portnumber"));

    // Process the actual command line arguments given by the user
    parser.process(*QApplication::instance());

    // Determine port number
    if (const uint32_t port_num = parser.value("port").toUInt(); port_num > 0 && port_num <= kMaxListenPort)
    {
        // Valid command line option - use command line port
        router_port_ = port_num;
        configuration_window_->EnableChangingPort(false);
    }
    else
    {
        // No valid command line option. Use the port specified in user settings file.
        router_port_ = settings_->GetListenPort();
    }

    if (previous_router_port_ == static_cast<uint32_t>(-1))
    {
        previous_router_port_ = router_port_;
    }

    emit RouterPortUpdated(static_cast<int>(router_port_));

    // Create router
    DDRouterCreateInfo info = {};
    info.pDescription       = kRadeonDeveloperServiceFilename;
    info.remotePort         = router_port_;

    if (DD_RESULT result = ddRouterCreate(&info, &router_); result == DD_RESULT_SUCCESS)
    {
        previous_router_port_ = router_port_;
        emit Connected();

        result = ddRouterLoadBuiltinModule(router_, SystemTraceQueryModule(), nullptr);
        Q_ASSERT(result == DD_RESULT_SUCCESS);

        result = ddRouterLoadBuiltinModule(router_, RouterUtilsQueryModuleInterface(), nullptr);
        Q_ASSERT(result == DD_RESULT_SUCCESS);

        result = ddRouterLoadBuiltinModule(router_, SiphonQueryModuleInterface(), nullptr);
        Q_ASSERT(result == DD_RESULT_SUCCESS);
    }
    else
    {
        emit ConnectFailed(static_cast<unsigned int>(router_port_));

        if (previous_router_port_ != router_port_)
        {
            // Reset the port
            settings_->SetListenPort(previous_router_port_);

            emit RouterPortUpdated(static_cast<int>(previous_router_port_));
            OnRouterEndpointUpdated();
        }
    }
}

void MainWindow::ShutdownService()
{
    ddRouterDestroy(router_);
    router_ = DD_API_INVALID_HANDLE;
}

void MainWindow::OnTerminateProcessEmitted()
{
    qApp->quit();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
}

void MainWindow::OnRouterEndpointUpdated()
{
    // Shutdown the service.
    ShutdownService();

    // Start the service again. Endpoint configuration is loaded from the user's settings file.
    InitializeService();
}

//-----------------------------------------------------------------------------
/// Handler invoked when the system tray icon is somehow activated.
/// \param reason The reason why this handler was invoked.
//-----------------------------------------------------------------------------
void MainWindow::OnTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::ActivationReason::DoubleClick)
    {
        ToggleConfigWindowVisibility();
    }
}

//-----------------------------------------------------------------------------
/// Show or hide the configuration window.  Adjust position if its not fully viewable.
//-----------------------------------------------------------------------------
void MainWindow::ToggleConfigWindowVisibility() const
{
    if (configuration_window_ != nullptr)
    {
        if (configuration_window_->isHidden() || configuration_window_->isMinimized())
        {
            const QRect current_geometry = configuration_window_->geometry();
            QPoint      top_left         = QGuiApplication::screens().first()->availableGeometry().topLeft();
            if (const QWindow* window_handle = configuration_window_->windowHandle(); window_handle != nullptr)
            {
                if (const QScreen* screen = window_handle->screen(); screen != nullptr)
                {
                    top_left = screen->availableGeometry().topLeft();
                }
            }

#ifdef Q_OS_WIN
            // Note: For Windows, the dialog needs to be positioned to get the titlebar on screen the
            // first time it is displayed.  The initial titlebar height is 0 for Windows.  Linux does
            // not have this issue.
            const int titlebar_height = configuration_window_->frameGeometry().height() - current_geometry.height();
#else
            const int titlebar_height = QApplication::style()->pixelMetric(QStyle::PM_TitleBarHeight);
#endif
            if (titlebar_height == 0 || (current_geometry.x() < top_left.x()) || (current_geometry.y() < top_left.y()))
            {
                configuration_window_->move(top_left.x(), top_left.y());
            }

            configuration_window_->showNormal();
            configuration_window_->setFocus();
        }
        else
        {
            configuration_window_->hide();
        }
    }
}

//-----------------------------------------------------------------------------
/// Open the configuration window.
//-----------------------------------------------------------------------------
void MainWindow::OnConfigureTriggered() const
{
    ToggleConfigWindowVisibility();
}
