// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the supported connection set.

#ifndef RDP_SOURCE_TRACE_SRC_SUPPORTED_CONNECTION_SET_H_
#define RDP_SOURCE_TRACE_SRC_SUPPORTED_CONNECTION_SET_H_

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <dd_common_api.h>

#include "client_connections.h"
#include "dev_trace_common.h"

namespace devtrace
{
    /// @brief Class that holds supported list of connections.
    class SupportedConnectionSet
    {
    public:
        /// @brief Constructor.
        /// @param [in] api_filter The list of APIs that are supported.
        explicit SupportedConnectionSet(const std::vector<Api>& api_filter);

        /// @brief Updates the current connections.
        /// @param [in] connections The current driver connections.
        /// @param [in] api_filter If not empty, the list of APIs.
        /// @param [in] handler Object to use to handle connections / disconnections.
        /// @param [out] new_connections Newly added connections.
        void UpdateConnections(const std::vector<ClientConnection>& connections,
                               ConnectionUpdateHandler*             handler,
                               std::vector<ClientConnection>*       new_connections = nullptr);

        /// @brief Creates a mapping of each connection to its API.
        ///
        /// The keys of the map will be unique identifiers of each connections.
        /// @param [out] mapping The mapping of the unique connection identifier to its API.
        /// @param [in] filter A filter to use to filter out clients.
        void BuildConnectionMapping(std::unordered_map<uint16_t, Api>& mapping, const ClientConnectionFilter& filter = {});

        /// @brief Gets the current supported connections.
        /// @return The current supported connections.
        [[nodiscard]] const std::vector<ClientConnection>& GetCurrentSupportedConnections();

        /// @brief Returns true if there are any connections -- supported or unsupported.
        /// @return true if there are any connections.
        [[nodiscard]] bool IsConnected();

        /// @brief Returns true if there are connections, but none of them are supported.
        /// @return true if there are connections, but none of them are supported.
        [[nodiscard]] bool IsConnectedButNotSupported();

        /// @brief Returns whether or not the connection has reached post device init.
        /// @param [in] umd_connection_id The umd connection id.
        /// @return true if the connection has reached post device init, false otherwise.
        [[nodiscard]] bool HasConnectionReachedPostDeviceInit(DDConnectionId umd_connection_id);

        /// @brief Sets the API filter.
        ///
        /// This will also re-filter the connections using UpdateConnections() and the provided handler.
        /// @param [in] filter The new filter to use.
        /// @param [in] handle The handler to use while re-filtering the connections.
        void SetApiFilter(const std::vector<Api>& filter, ConnectionUpdateHandler* handler);

    private:
        std::recursive_mutex connection_mutex_;  ///< Mutex that guards connections.

        std::vector<ClientConnection> unfiltered_connections_;  ///< All of the current connections (unfiltered).
        std::vector<ClientConnection> filtered_connections_;    ///< All of the current connections (filtered).

        std::vector<Api> api_filter_;  ///< The list of APIs that are supported.
    };
}  // namespace devtrace

#endif
