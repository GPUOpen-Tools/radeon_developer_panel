// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  MainWindow class definition

#ifndef RDP_SOURCE_SERVICE_GUI_MAIN_WINDOW_H_
#define RDP_SOURCE_SERVICE_GUI_MAIN_WINDOW_H_

#include <memory>

#include <QMainWindow>
#include <QSystemTrayIcon>

#include <ddRouter.h>

#include "configuration_window.h"
#include "settings.h"

namespace Ui
{
    class MainWindow;
}

/// @brief Defines service main application window
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    /// @brief Constructor
    /// @param [in] parent The parent widget
    explicit MainWindow(QWidget* parent = Q_NULLPTR);

    /// @brief Destructor
    ~MainWindow() Q_DECL_NOEXCEPT Q_DECL_OVERRIDE;

protected:
    void showEvent(QShowEvent* event) Q_DECL_OVERRIDE;

signals:
    /// @brief Signal to terminate process
    void TerminateProcess();

    /// @brief Signal router port updated
    /// @param [in] port The new router port
    void RouterPortUpdated(int port);

    /// @brief Emitted when RDS cannot be opened on the specified port.
    /// @param port The port that RDS was tried to be opened on.
    void ConnectFailed(unsigned int port);

    /// @brief Emitted when RDS connects successfully.
    void Connected();

private slots:
    /// @brief Handle response to process terminate
    static void OnTerminateProcessEmitted();

    /// @brief Handle response to router endpoint updated
    void OnRouterEndpointUpdated();

    /// @brief Handler invoked when the system tray icon is activated.
    /// @param [in] reason The reason why this handler was invoked.
    void OnTrayIconActivated(QSystemTrayIcon::ActivationReason reason);

    /// @brief Handle response to configure action triggered
    void OnConfigureTriggered() const;

private:
    /// @brief Creates actions
    void CreateActions();

    /// @brief Creates application tray icon
    void CreateTrayIcon();

    /// @brief Initializes router service
    void InitializeService();

    /// @brief Shuts down router service
    void ShutdownService();

    /// @brief Toggles config window visibility
    void ToggleConfigWindowVisibility() const;

private:
    std::unique_ptr<Ui::MainWindow> ui_;                    ///< Qt ui
    Settings*                       settings_;              ///< Application settings
    QAction*                        quit_action_;           ///< Action to terminate the listener application.
    QAction*                        configure_action_;      ///< Action to open the configuration window.
    QMenu*                          tray_icon_menu_;        ///< Context Menu opened on right-clicking tray icon.
    QSystemTrayIcon*                tray_icon_;             ///< The listener's system tray icon.
    ConfigurationWindow*            configuration_window_;  ///< The RDS settings configuration window.
    DDRouter                        router_;                ///< ddRouter handle
    uint32_t                        router_port_;           ///< The port that the router has opened for incoming connections.
    uint32_t                        previous_router_port_;  ///< The last used port before any changes.
};

#endif
