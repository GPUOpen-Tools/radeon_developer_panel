// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Connection UI class definition

#ifndef RDP_SOURCE_FRONTEND_CONNECTION_WIDGET_H_
#define RDP_SOURCE_FRONTEND_CONNECTION_WIDGET_H_

#include <memory>

#include <QVariant>
#include <QWidget>

#include "models/application_model.h"
#include "models/connection_model.h"
#include "settings_manager.h"
#include "tool_wrapper_types.h"

class QSettings;

namespace Ui
{
    class ConnectionWidget;
}

namespace rdp
{
    /// @brief Widget for establishing a ddTool connection
    class ConnectionWidget : public QWidget
    {
        Q_OBJECT
    public:
        /// @brief explicit Constructor
        /// @param [in] settings_manager The object that manages RDP application settings.
        /// @param [in] connection_model The model used to manage connections with applications.
        /// @param [in] timeout_dialog The dialog used to edit the custom DD timeouts.
        /// @param [in] parent The parent widget
        explicit ConnectionWidget(std::shared_ptr<SettingsManager>            settings_manager,
                                  std::weak_ptr<ConnectionModel>              connection_model,
                                  const std::shared_ptr<class TimeoutDialog>& timeout_dialog,
                                  QWidget*                                    parent = nullptr);

        /// @brief Destructor
        ~ConnectionWidget() Q_DECL_OVERRIDE;

    private:
        /// Loads the connection history from settings file
        void LoadConnectionHistoryFromSettings();

    public:
        /// @brief Query UI for local connection info
        /// @returns local connection info instance
        static LocalConnectionInfo QueryLocalConnectionInfo();

        /// @brief Query UI for remote connection info
        /// @returns remote connection info instance
        RemoteConnectionInfo QueryRemoteConnectionInfo() const;

        /// @brief Attempt an auto-connect to connection
        void AttemptAutoConnection();

        /// @brief Attempt new remote connection
        ///
        /// Triggers a connection attempt using the remote
        /// connection info passed in
        void AttemptNewRemoteConnection(const QString& target);

        /// @brief Represents a single item in the connection history
        struct HistoryItem
        {
            /// @brief connection info of type (LocalConnectionInfo || RemoteConnectionInfo)
            QVariant connection_info;

            /// @brief Default constructor
            HistoryItem() = default;

            /// @brief Constructor specifying a local connection
            /// @param [in] connection_info the local connection info
            explicit HistoryItem(const LocalConnectionInfo& connection_info);

            /// @brief Constructor specifying a remote connection
            /// @param [in] connection_info the remote connection info
            explicit HistoryItem(const RemoteConnectionInfo& connection_info);

            /// @brief equality operator overload
            /// Compares two history item connections
            /// @return true if equal, false otherwise
            bool operator==(const HistoryItem& item) const;

            /// @brief Gets the connection type of item
            /// @return connection type
            ConnectionType GetType() const;
        };

    public slots:

        /// @brief Called when the application model loads.
        /// @param model The application model.
        void OnApplicationModelLoaded(std::shared_ptr<ApplicationModel> model);

    private slots:
        /// @brief Called when the connection state changes to disconnecting.
        void NetDisconnecting();

        /// @brief Called when the connection state changes to disconnected.
        void NetDisconnected();

        /// @brief Called when the connection state changes to disconnected unexpectedly.
        void OnNetDisconnectedUnexpectedly();

        /// @brief Called when the connection state changes to connecting.
        /// @param [in] description The description of the pending connection.
        void NetConnecting(const QString& description);

        /// @brief Called when the connection state changes to connected.
        /// @param [in] description The description of the current connection.
        void NetConnected(const QString& description);

        /// @brief Called when trying to connect locally, but the router creation failed.
        void OnRouterCreationFailed();

        /// @brief Called when trying to connect, but an existing local router was found.
        void OnExistingLocalRouterFound();

        /// @brief Slot handling connect button clicked
        ///
        /// Triggers a connection attempt using the current
        /// connection info settings
        void OnConnectButtonClicked();

        /// @brief Slot handling disconnect button clicked.
        /// Triggers a disconnect from active connection.
        void OnDisconnectButtonClicked();

        /// @brief Called when the state of the disable client timeout box changes.
        /// @param [in] checked the state of the disable client timeout checkbox.
        void OnDisableClientTimeoutStateChanged(bool checked);

        /// @brief Slot handling saving of settings
        ///
        /// Saves the connection history to settings file
        /// @param [in] settings The settings object
        bool OnSaveSettings(QSettings* settings) const;

        /// @brief Slot handling selection change in history dropdown
        ///
        /// Handles the selection of connection in connection
        /// list
        /// @param [in] index The selected index
        void OnHistoryItemSelected(int index);

        /// @brief Called when a client connects.
        void OnClientConnect();

        /// @brief Called when a client disconnects.
        void OnClientDisconnect();

        /// @param Opens the timeout dialog.
        void ShowTimeoutDialog();

    private:
        ///< Default connection port
        constexpr static const char* kSettingsPrefix = "ConnectionWidget";  ///< Settings group prefix

        /// @brief Validates host name input value
        /// @returns True is host name is valid. False otherwise
        bool ValidateHostName() const;

        std::unique_ptr<Ui::ConnectionWidget> ui_;                 ///< Qt designer UI handle
        std::weak_ptr<ConnectionModel>        connection_model_;   ///< Model used to manage connections with applications.
        std::shared_ptr<ApplicationModel>     application_model_;  ///< Model used to manage the lifecycle of connections with applications.
        std::shared_ptr<class TimeoutDialog>  timeout_dialog_;     ///< The dialog used to edit the custom DD timeouts.

        ConnectionType current_connection_type_;  ///< Currently selected connection type
        int            last_used_index_;          ///< The last used connection index in history
        bool           enabled_auto_connect_;     ///< Enabled unless overwritten by settings

        std::shared_ptr<SettingsManager> settings_manager_;  ///< Manages the RDP application settings.
    };

    inline bool ConnectionWidget::HistoryItem::operator==(const HistoryItem& item) const
    {
        bool equal = false;

        if (this->connection_info.userType() == item.connection_info.userType())
        {
            if ((this->GetType() == ConnectionType::kLocal) && (item.GetType() == ConnectionType::kLocal))
            {
                equal = true;
            }
            else if ((this->GetType() == ConnectionType::kRemote) && (item.GetType() == ConnectionType::kRemote))
            {
                const auto& self = this->connection_info.value<RemoteConnectionInfo>();
                const auto& rhs  = item.connection_info.value<RemoteConnectionInfo>();

                equal = ((self.hostname == rhs.hostname) && (self.port == rhs.port));
            }
            else
            {
                const std::string what = QString("Got a bad type stored in a HistoryItem: %1, %2")
                                             .arg(this->connection_info.typeName(), item.connection_info.typeName())
                                             .toStdString();
                Q_ASSERT_X(false, __FUNCTION__, what.c_str());
            }
        }

        return equal;
    }

    inline ConnectionType ConnectionWidget::HistoryItem::GetType() const
    {
        ConnectionType type = ConnectionType::kUnknown;

        if (this->connection_info.canConvert<LocalConnectionInfo>())
        {
            type = ConnectionType::kLocal;
        }
        else if (this->connection_info.canConvert<RemoteConnectionInfo>())
        {
            type = ConnectionType::kRemote;
        }

        return type;
    }

}  // namespace rdp

Q_DECLARE_METATYPE(rdp::ConnectionWidget::HistoryItem)

#endif
