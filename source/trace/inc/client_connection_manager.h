// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the object responsible for managing client connections.

#ifndef RDP_SOURCE_TRACE_INC_CLIENT_CONNECTION_MANAGER_H_
#define RDP_SOURCE_TRACE_INC_CLIENT_CONNECTION_MANAGER_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include <ddApi.h>
#include <dd_common_api.h>

#include "client_connections.h"
#include "dipper.h"

struct DDApiRegistry;
struct DDConnectionCallbacksImpl;
struct DDConnectionInfo;
struct DDConnectionApi;

namespace devtrace
{
    /// @brief An object that is responsible for managing client connections.
    class ClientConnectionManager
    {
    public:
        /// @brief Destructor.
        ~ClientConnectionManager();

        /// @brief Constructor.
        /// @param [in] connection_api The API that the manager registers and unregisters with.
        DIP(ClientConnectionManager(DDConnectionApi* connection_api));

        /// @brief Initializes the client connection manager.
        /// @param [in] subscriber The subscriber that this manager should update.
        /// @return true if initialization was successful, false otherwise.
        bool Initialize(const std::shared_ptr<class ClientConnectionSubscriber>& subscriber);

        /// @brief Sets whether or not the connection manager is enabled.
        /// @param [in] enabled true if the connection manager should be enabled, false otherwise.
        void SetEnabled(bool enabled);

    private:
        /// @brief Called when a client connects.
        /// @param [in] userdata The model to notify of the connection.
        /// @param [in] connection_info The connection info of the client.
        static void OnDriverConnected(DDConnectionCallbacksImpl* userdata, const DDConnectionInfo* connection_info);

        /// @brief Called when a client connects.
        /// @param [in] userdata The model to notify of the connection.
        /// @param [in] umd_connection_id The identifier of the UMD connection.
        static void OnDriverDisconnected(DDConnectionCallbacksImpl* userdata, DDConnectionId umd_connection_id);

        /// @brief Called when the state of the driver changes for a client.
        /// @param [in] userdata The model to notify of the connection.
        /// @param [in] umd_connection_id The identifier of the UMD connection.
        /// @param [in] state The state of the driver for the client.
        static void OnDriverStateChanged(DDConnectionCallbacksImpl* userdata, DDConnectionId umd_connection_id, DD_DRIVER_STATE state);

        /// @brief Called when a client connects.
        /// @param [in] connection_info The connection info of the client.
        void OnDriverConnected(const DDConnectionInfo& connection_info);

        /// @brief Called when a client connects.
        /// @param [in] umd_connection_id The identifier of the UMD connection.
        void OnDriverDisconnected(DDConnectionId umd_connection_id);

        /// @brief Called when the state of the driver changes for a client.
        /// @param [in] umd_connection_id The identifier of the UMD connection.
        /// @param [in] state The state of the driver for the client.
        void OnDriverStateChanged(DDConnectionId umd_connection_id, DD_DRIVER_STATE state);

        /// @brief Returns true if there is an existing connection for the the given UMD connection id.
        /// @param [in] umd_connection_id The identifier of the UMD connection.
        /// @return true if there is an existing connection, false otherwise.
        bool HasExistingConnection(DDConnectionId umd_connection_id);

    private:
        DDConnectionApi* connection_api_ = nullptr;  ///< The API that the manager registers and unregisters with.
        bool             is_enabled_     = false;    ///< true if the connection manager is enabled, false otherwise.

        std::shared_ptr<class ClientConnectionSubscriber> subscriber_;           ///< The subscriber should update.
        std::vector<ClientConnection>                     pending_connections_;  ///< The pending connections.
        std::vector<ClientConnection>                     current_connections_;  ///< The current connections.
        std::mutex                                        connection_mutex_;     ///< The mutex that guards the subscriber and current connections.
    };

    class ClientConnectionManagerV2
    {
    public:
        DIP(ClientConnectionManagerV2(DDConnectionApi* connection_api));

        /// @brief Initializes the client connection manager.
        /// @param [in] api_registry The API registry to use to initialize the client connection manager.
        /// @param [in] subscriber The subscriber that this manager should update.
        /// @return true if initialization was successful, false otherwise.
        bool Initialize(const std::shared_ptr<class ClientConnectionSubscriberV2>& subscriber);

        /// @brief Destructor.
        ~ClientConnectionManagerV2();

        /// @brief Sets whether or not the connection manager is enabled.
        /// @param [in] enabled true if the connection manager should be enabled, false otherwise.
        void SetEnabled(bool enabled);

    private:
        /// @brief Called when a client connects.
        /// @param [in] userdata The model to notify of the connection.
        /// @param [in] connection_info The connection info of the client.
        static void OnDriverConnected(DDConnectionCallbacksImpl* userdata, const DDConnectionInfo* connection_info);

        /// @brief Called when a client connects.
        /// @param [in] userdata The model to notify of the connection.
        /// @param [in] umd_connection_id The identifier of the UMD connection.
        static void OnDriverDisconnected(DDConnectionCallbacksImpl* userdata, DDConnectionId umd_connection_id);

        /// @brief Called when the state of the driver changes for a client.
        /// @param [in] userdata The model to notify of the connection.
        /// @param [in] umd_connection_id The identifier of the UMD connection.
        /// @param [in] state The state of the driver for the client.
        static void OnDriverStateChanged(DDConnectionCallbacksImpl* userdata, DDConnectionId umd_connection_id, DD_DRIVER_STATE state);

        /// @brief Handle response to router connection.
        /// @param [in] userdata manager.
        /// @param [in] connection_id Client connection id.
        static void OnRouterConnected(DDConnectionCallbacksImpl* userdata, DDConnectionId connection_id);

        /// @brief Handle response to router disconnect.
        /// @param [in] userdata manager.
        static void OnRouterDisconnected(DDConnectionCallbacksImpl* userdata);

    private:
        DDConnectionApi* connection_api_ = nullptr;  ///< The API that the manager registers and unregisters with.
        std::atomic_bool is_enabled_     = false;    ///< true if the connection manager is enabled, false otherwise.

        std::shared_ptr<class ClientConnectionSubscriberV2> subscriber_;  ///< The subscriber should update.
    };

}  // namespace devtrace

#endif
