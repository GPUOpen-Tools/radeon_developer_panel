// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the object responsible for managing client connections.

#ifndef RDP_SOURCE_TRACE_INC_CLIENT_CONNECTIONS_H_
#define RDP_SOURCE_TRACE_INC_CLIENT_CONNECTIONS_H_

#include <algorithm>
#include <functional>
#include <vector>

#include <dd_common_api.h>

#include "dev_trace_common.h"

struct DDConnectionInfo;

namespace devtrace
{

    /// @brief The information about a connection to a driver client.
    struct ClientConnection
    {
        Api            api               = Api::kUnknown;  ///< The API that the client is using.
        DDConnectionId umd_connection_id = 0;              ///< The identifier of the UMD connection for the client.
        uint32_t       client_pid        = 0;              ///< The process id of the connected client.
        std::string    client_name       = "";             ///< The name of the client process as ASCII. Not unique.

        // TODO: remove when all the trace sources are built on V2
        bool has_reached_post_device_init = false;  ///< true if the post device init state has been reached.
    };

    /// @brief An object that can response to connection changes.
    class ConnectionUpdateHandler
    {
    public:
        virtual ~ConnectionUpdateHandler() = default;

        /// @brief Handles a new connection.
        /// @param [in] connection The new connection.
        virtual void HandleNewConnection(const ClientConnection& connection) = 0;

        /// @brief Handles a disconnect.
        /// @param [in] connection The connection that disconnected.
        virtual void HandleDisconnect(const ClientConnection& connection) = 0;

        /// @brief Called after a connection reaches post device init.
        /// @param [in] connection The connection that reached post device init.
        virtual void ConnectionReachedPostDeviceInit([[maybe_unused]] const ClientConnection& connection)
        {
        }
    };

    /// @brief Interface for something that should be notified of new connections and disconnections.
    class ClientConnectionSubscriber
    {
    public:
        virtual ~ClientConnectionSubscriber() = default;

        /// @brief Updates the currently connected clients.
        /// @param [in] connections The current connections.
        virtual void UpdateClientConnections(const std::vector<ClientConnection>& connections) = 0;

        /// @brief Returns the driver state that a client should be considered as connected on.
        /// @return The driver state that a client should be considered as connected on.
        [[nodiscard]] virtual DD_DRIVER_STATE GetConnectingDriverState() const
        {
            return DD_DRIVER_STATE_POSTDEVICEINIT;
        }
    };

    class ClientConnectionSubscriberV2
    {
    public:
        virtual ~ClientConnectionSubscriberV2() = default;

        /// @brief Handles new driver connections.
        /// @param [in] connection_info The new connection's information.
        virtual void OnDriverConnected(const DDConnectionInfo& connection_info) = 0;

        /// @brief Handles when a client disconnects.
        /// @param [in] umd_connection_id The UMD connection id of the client disconnecting.
        virtual void OnDriverDisconnected(DDConnectionId umd_connection_id) = 0;

        /// @brief Handles a driver state changing for a connected client.
        /// @param [in] umd_connection_id The UMD connection id of the client whose driver state changed.
        /// @param [in] state The new driver state of the client.
        virtual void OnDriverStateChanged(DDConnectionId umd_connection_id, DD_DRIVER_STATE state) = 0;

        /// @brief Called when the router connection state changes.
        /// @param [in] is_connected true if the router is connected, false otherwise.
        virtual void RouterConnectionStatusChanged([[maybe_unused]] bool is_connected) {};
    };

    /// @brief Returns true if a client should be included in some operation.
    using ClientConnectionFilter = std::function<bool(const ClientConnection&)>;

    /// Finds the connection with the connection ID.
    /// @param [in] connections The connections to search in.
    /// @param [in] umd_connection_id The UMD ID of the connection to find.
    /// @return An iterator to the connection with the given connection ID or connections.end() if no matching connection was present.
    inline std::vector<ClientConnection>::iterator FindConnection(std::vector<ClientConnection>& connections, const DDConnectionId umd_connection_id)
    {
        return std::ranges::find_if(connections, [=](const ClientConnection& connection) { return connection.umd_connection_id == umd_connection_id; });
    }

}  // namespace devtrace

#endif
