// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definitions for model that manages tool connections.

#ifndef RDP_SOURCE_FRONTEND_MODELS_CONNECTION_MODEL_H_
#define RDP_SOURCE_FRONTEND_MODELS_CONNECTION_MODEL_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include <ddRouter.h>
#include <dd_api_registry_api.h>
#include <dd_connection_api.h>

#include "models/api_model.h"
#include "models/module_model.h"
#include "tool_wrapper_types.h"

namespace rdp
{
    /// @brief Model that manages connections inside a tool context.
    class ConnectionModel final : public QObject
    {
        Q_OBJECT

        /// @brief Called when a DDTool connects to a router.
        /// @param [in] instance The connection model that registered the callback.
        /// @param [in] connection_id The id that uniquely represents the connection to DDRouter.
        static void OnRouterConnected(DDConnectionCallbacksImpl* instance, DDConnectionId connection_id);

        /// @brief Called when a DDTool disconnects from a router.
        /// @param [in] instance The connection model that registered the callback.
        static void OnRouterDisconnected(DDConnectionCallbacksImpl* instance);

    public:
        /// @brief Constructor.
        /// @param [in] module_model Module that manages models.
        /// @param [in] settings_manager The object that manages the application settings.
        /// @param [in] timeout_model The model that handles custom DD timeouts.
        ConnectionModel(const std::shared_ptr<ModuleModel>&        module_model,
                        const std::shared_ptr<SettingsManager>&    settings_manager,
                        const std::shared_ptr<class TimeoutModel>& timeout_model);

        /// @brief De-initializes model.
        void ShutDown();

        /// @brief Creates a ddTool instance
        /// @return true if the tool creation was successful, false otherwise.
        bool CreateTool();

        /// @brief To be called when a view binds to this model.
        void OnBind();

        /// @brief Gets the API registry for the current DDTool.
        /// @return The API registry for the current DDTool.
        DDApiRegistry* GetApiRegistry() const;

        /// @brief Gets the connection type for the active connection
        /// @return connection type
        [[nodiscard]] ConnectionType GetActiveConnectionType() const;

    private:
        /// @brief Creates the router.
        /// @return The result of the router creation.
        DD_RESULT CreateRouter();

        /// @brief Destroys the router.
        void DestroyRouter();

    public:
        /// @brief Loads the modules.
        /// @return true if the module loading was successful, false otherwise.
        bool LoadModules() const;

        /// @brief Synchronously connect to a local tool
        /// @param [in] use_existing_local_router true if the connection should use an existing local router.
        void ConnectToolLocal(bool use_existing_local_router = false);

        /// @brief Synchronously connect to a remote tool
        /// @param [in] connection_info The remote connection info
        void ConnectToolRemote(const RemoteConnectionInfo& connection_info);

    private:
        /// @brief Connects the tool asynchronously.
        /// @param [in] ip_str The IP of the router to connect the DDTool to, nullptr for local.
        /// @param [in] port The port of the router to connect the DDTool to, 0 for local.
        /// @param [in] description The description of the connection.
        /// @param [in] connection_type The type of connection.
        /// @param [in] do_not_create_router true if a local router should not be created in any case.
        void ConnectToolAsync(const char* ip_str, uint32_t port, const QString& description, ConnectionType connection_type, bool do_not_create_router = false);

        /// @brief Connects the tool.
        /// @param [in] ip_str The IP of the router to connect the DDTool to, nullptr for local.
        /// @param [in] port The port of the router to connect the DDTool to, 0 for local.
        /// @param [in] description The description of the connection.
        /// @param [in] connection_type The type of connection.
        /// @param [in] do_not_create_router true if a local router should not be created in any case.
        void ConnectTool(const char* ip_str, uint32_t port, const QString& description, ConnectionType connection_type, bool do_not_create_router = false);

    signals:

        /// @brief Emitted when trying to connect locally, but the router creation failed.
        void RouterCreationFailed();

        /// @brief Emitted when trying to create a router, but there is already an existing one on the system.
        void ExistingLocalRouterFound();

    public:
        /// @brief Asynchronously disconnect from a tool
        void DisconnectTool();

    private:
        /// @brief Synchronously disconnect from a tool
        void SyncDisconnectTool();

        /// @brief Called when the router is actually disconnected.
        void OnRouterDisconnected();

    public:
        /// @brief Returns true if there is an active connection to the net bus, false otherwise.
        /// @return true if there is an active connection to the net bus, false otherwise.
        bool IsConnected();

        /// @brief Sets whether this should disable the client timeout when the router is created.
        ///
        /// If the router is currently connected, this will not affect the timeout. The router will need to be disconnected
        /// or reconnected. If you want to force the router to have the client timeout be enabled / disabled, you should
        /// emit a signal into OnDisableClientTimeoutToggled.
        ///
        /// @param disable_client_timeout true if the client timeout should be disabled.
        void SetDisableClientTimeout(bool disable_client_timeout);

        /// @brief Returns whether the client timeout can be disabled or not for the current connection.
        /// @return true if the client timeout can be disabled for the current connection. false otherwise.
        bool CanClientTimeoutBeDisabledForCurrentConnection();

        /// @brief Returns whether the client timeout should be disabled.
        ///
        /// If SetDisableClientTimeout() was called and the router was not destroyed and reconnected,
        /// the return value will not reflect the setting of the current router, but instead the last
        /// value that SetDisableClientTimeout() was called with.
        /// @return true if the client timeout should be disabled on the router.
        bool GetIsClientTimeoutDisabled();

    signals:
        /// @brief Emitted when the connection state changes to disconnecting.
        void NetDisconnecting();

        /// @brief Emitted when the connection state changes to disconnected.
        void NetDisconnected();

        /// @brief Emitted when the connection state changes to disconnected unexpectedly.
        void NetDisconnectedUnexpectedly();

        /// @brief Emitted when the connection state changes to connecting.
        /// @param [in] description The description of the pending connection.
        void NetConnecting(const QString& description);

        /// @brief Emitted when the connection state changes to connected.
        /// @param [in] description The description of the current connection.
        /// @param [in] was_reconnection true if the connection was reconnecting.
        void NetConnected(const QString& description, bool was_reconnection);

    private:
        /// @brief The different connections states.
        enum class ConnectionState : uint8_t
        {
            kDisconnected = 0,  ///< There is no valid connection and no attempt to connect is in progress
            kDisconnecting,     ///< There is a valid connection, but it is being disconnected
            kConnecting,        ///< There is no valid connection, but there is an attempt to connect in progress
            kConnected          ///< There is a valid connection
        };

        std::shared_ptr<ModuleModel>     module_model_;      ///< Model that manages modules.
        std::shared_ptr<SettingsManager> settings_manager_;  ///< Object that manages app settings.
        std::shared_ptr<TimeoutModel>    timeout_model_;     ///< The model responsible for custom DD timeouts.

        std::mutex       tool_context_mutex_;        ///< The mutex that guards access to the tool context.
        DDToolApi*       tool_api_       = nullptr;  ///< API used to manage DDTool.
        DDConnectionApi* connection_api_ = nullptr;  ///< The API used to connect and disconnect a router.

        std::mutex router_mutex_;                    ///< The mutex that guards access to the router.
        DDRouter   router_ = DD_API_INVALID_HANDLE;  ///< The handle to the router.

        std::recursive_mutex connection_mutex_;        ///< The mutex that guards the connection.
        QString              connection_description_;  ///< The description of the current connection.
        std::atomic<bool>    should_reconnect_;        ///< true if the tool should be reconnected when it is disconnected.
        std::atomic<bool>    currently_reconnecting_;  ///< true if the pending connection is a reconnection.

        std::thread connection_thread_;  ///< The thread that all connecting / disconnecting is performed on.

        ConnectionState connection_state_       = ConnectionState::kDisconnected;  ///< The current state of the DDTool connection.
        ConnectionType  active_connection_type_ = ConnectionType::kUnknown;        ///< Cache of current connection type.
        bool            disable_client_timeout_ = false;                           ///< Whether to disable the client timeout on the router.
    };

}  // namespace rdp

#endif
