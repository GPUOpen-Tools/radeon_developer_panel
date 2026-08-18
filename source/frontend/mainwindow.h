// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP main window class definition

#ifndef RDP_SOURCE_FRONTEND_MAINWINDOW_H_
#define RDP_SOURCE_FRONTEND_MAINWINDOW_H_

#include <memory>

#include <QItemSelectionModel>
#include <QLabel>
#include <QMainWindow>

#include <qt_common/custom_widgets/message_overlay_container.h>
#include <qt_common/custom_widgets/navigation_list_model.h>

#include "bottom_bar_tab_widget.h"
#include "bug/bug_report_generator.h"
#include "crash/crash_handler.h"
#include "models/api_proxy_model.h"
#include "models/application_model.h"
#include "models/application_proxy_model.h"
#include "models/blocklist_model.h"
#include "models/connection_model.h"
#include "models/system_info_model.h"
#include "settings_manager.h"

namespace Ui
{
    class MainWindow;
}

namespace rdp
{
    class ConnectionWidget;

    /// @brief RDP main application window
    class MainWindow : public QMainWindow, public BottomBarTabWidgetDelegate
    {
        Q_OBJECT
    public:
        /// @brief Constructor
        /// @param [in] module_model The model that manages the modules.
        /// @param [in] timeout_model The model that handles custom DD timeouts.
        /// @param [in] connection_model The model that manages the system information.
        /// @param [in] connection_model The model that manages connections to applications.
        /// @param [in] settings_manager The manager of the RDP application settings.
        /// @param [in] parent The parent widget
        MainWindow(const std::shared_ptr<class ModuleModel>&  module_model,
                   const std::shared_ptr<class TimeoutModel>& timeout_model,
                   const std::shared_ptr<ConnectionModel>&    connection_model,
                   const std::shared_ptr<SystemInfoModel>&    system_info_model,
                   const std::shared_ptr<SettingsManager>&    settings_manager,
                   QWidget*                                   parent = nullptr);

    private:
        /// @brief Initializes all of the models.
        void InitializeModels();

    public:
        /// @brief Destructor
        ~MainWindow() override;

        /// @brief Attempt to automatically connect to a remote address
        /// @param [in] target Remote address
        void AttemptNewRemoteConnection(const QString& target);

        /// @brief Attempt to automatically connect to the default connection
        void AttemptAutoConnection();

    public slots:
        /// @brief Handle bringing window to foreground
        void BringToForeground();

    private slots:

        /// @brief Called when the connection state changes to disconnecting.
        void NetDisconnecting();

        /// @brief Called when the connection state changes to disconnected.
        void NetDisconnected();

        /// @brief Called when the connection state changes to connecting.
        void NetConnecting();

        /// @brief Called when the connection state changes to connected.
        /// @param [in] description The description of the current connection.
        /// @param [in] was_reconnection true if the connection was reconnecting.
        void NetConnected(const QString& description, bool was_reconnection);

        /// @brief Called when the system info model fails to load.
        static void OnSystemInfoFailedToLoad();

        /// @brief Called when an application tries to connect, but an application is already connected.
        /// @param [in] app_name The name of the application that tried to connect.
        /// @param [in] pid The PID of the process.
        void OnClientConnectedWhenAlreadyConnected(const QString& app_name, qint64 pid);

        /// @brief Called when the system info model loads if the ETW status code was nonzero.
        /// @param [in] has_permission true if the permissions to use ETW were granted, false otherwise.
        void OnEtwStatusCodeNonzero(bool has_permission);

        /// @brief Called when system info model signals user group script must be run
        void OnAddUserToGroup();

        /// @brief Called when using handheld device with unsupported driver.
        void OnUnsupportedHandheldDriver();

        /// @brief Called when the tab changes, including settings.
        /// @param [in] The index of the tab that is now selected.
        void TabChanged(int index);

    private:
        /// @brief QWidget::closeEvent() override
        /// @param [in] event The close event
        void closeEvent(QCloseEvent* event) override;

    public:
        void SetBottomBarHeight(int new_height) override;

    private:
        enum class ShutdownState : uint32_t
        {
            kNotRequested = 0,
            kRequested,
            kReady
        };

        std::unique_ptr<Ui::MainWindow> ui_;  ///< Qt ui component

        std::shared_ptr<SettingsManager>   settings_manager_;   ///< Manages the RDP application settings.
        std::shared_ptr<class ModuleModel> module_model_;       ///< Model used to manage modules.
        std::shared_ptr<class PresetModel> preset_model_;       ///< Model used to manage presets.
        std::shared_ptr<ConnectionModel>   connection_model_;   ///< Model used to manage connections to applications.
        std::shared_ptr<ApiModel>          api_model_;          ///< Model used to manage apis.
        std::shared_ptr<ApplicationModel>  application_model_;  ///< Model used to manage applications.
        std::shared_ptr<BlocklistModel>    blocklist_model_;    ///< Model used to manage blocked applications.
        std::shared_ptr<ApiProxyModel>     api_proxy_model_;    ///< Proxy model on top of API model.

        std::shared_ptr<SystemInfoModel>    system_info_model_;                              ///< System info model
        std::unique_ptr<BugReportGenerator> bug_report_generator_;                           ///< Object to generate bug reports with.
        ConnectionWidget*                   connection_widget_;                              ///< Connection widget
        ShutdownState                       shutdown_state_ = ShutdownState::kNotRequested;  ///< shutdown state
        QIcon                               green_icon_;                                     ///< green light connection icon
        QIcon                               red_icon_;                                       ///< red light connection icon
        QIcon                               help_icon_;                                      ///< Question icon for help button.
        QIcon                               gear_icon_;                                      ///< Gear icon for settings button.
        QIcon                               bug_icon_;                                       ///< Bug icon for bug report button.
        QPushButton*                        settings_button_       = nullptr;                ///< The button used to allow the user to show the RDP settings.
        QLabel*                             driver_settings_label_ = nullptr;                ///< Label that says driver settings were modified.
        BottomBarTabWidget*                 bottom_tab_widget_     = nullptr;                ///< Bottom tab widget.
        class SystemTabWidget*              system_tab_widget_     = nullptr;                ///< System tab widget hosting system-category modules.
    };

}  // namespace rdp

#endif
