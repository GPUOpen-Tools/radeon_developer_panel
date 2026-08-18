// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the supported connection set.

#include "supported_connection_set.h"

#include "client_connections.h"

namespace devtrace
{
    SupportedConnectionSet::SupportedConnectionSet(const std::vector<Api>& api_filter)
        : api_filter_(api_filter)
    {
    }

    void SupportedConnectionSet::UpdateConnections(const std::vector<ClientConnection>& connections,
                                                   ConnectionUpdateHandler*             handler,
                                                   std::vector<ClientConnection>*       new_connections)
    {
        const std::lock_guard connection_lock(connection_mutex_);

        // Clear new connection list
        if (new_connections != nullptr)
        {
            new_connections->clear();
        }

        // Filter out all the connections
        std::vector<ClientConnection> new_filtered_connections{};
        for (const ClientConnection& connection : connections)
        {
            if (api_filter_.empty() || std::ranges::find(api_filter_, connection.api) != api_filter_.end())
            {
                new_filtered_connections.push_back(connection);

                if (const auto existing_connection = FindConnection(filtered_connections_, connection.umd_connection_id);
                    existing_connection == filtered_connections_.end())
                {
                    handler->HandleNewConnection(connection);

                    if (connection.has_reached_post_device_init)
                    {
                        handler->ConnectionReachedPostDeviceInit(connection);
                    }

                    if (new_connections != nullptr)
                    {
                        new_connections->push_back(connection);
                    }
                }
                else
                {
                    if (!existing_connection->has_reached_post_device_init && connection.has_reached_post_device_init)
                    {
                        handler->ConnectionReachedPostDeviceInit(connection);
                    }

                    // Update the existing connection if any of the data changed.
                    *existing_connection = connection;
                }
            }
        }

        // Determine which clients disconnected
        for (const auto& connection : filtered_connections_)
        {
            if (FindConnection(new_filtered_connections, connection.umd_connection_id) == new_filtered_connections.end())
            {
                handler->HandleDisconnect(connection);
            }
        }

        unfiltered_connections_ = connections;
        filtered_connections_   = std::move(new_filtered_connections);
    }

    void SupportedConnectionSet::BuildConnectionMapping(std::unordered_map<uint16_t, Api>& mapping, const ClientConnectionFilter& filter)
    {
        const std::lock_guard connection_lock(connection_mutex_);

        mapping.clear();

        for (const auto& connection : filtered_connections_)
        {
            if (filter && !filter(connection))
            {
                continue;
            }

            // We use the UMD connection id as the unique identifier for convenience, but we could eventually maintain a mapping if we
            // wanted to obscure this information.
            mapping.insert({connection.umd_connection_id, connection.api});
        }
    }

    const std::vector<ClientConnection>& SupportedConnectionSet::GetCurrentSupportedConnections()
    {
        const std::lock_guard connection_lock(connection_mutex_);
        return filtered_connections_;
    }

    bool SupportedConnectionSet::IsConnected()
    {
        const std::lock_guard connection_lock(connection_mutex_);
        return !unfiltered_connections_.empty();
    }

    bool SupportedConnectionSet::IsConnectedButNotSupported()
    {
        const std::lock_guard connection_lock(connection_mutex_);
        return IsConnected() && filtered_connections_.empty();
    }

    bool SupportedConnectionSet::HasConnectionReachedPostDeviceInit(const DDConnectionId umd_connection_id)
    {
        const std::lock_guard connection_lock(connection_mutex_);

        const auto connection = FindConnection(filtered_connections_, umd_connection_id);
        if (connection == filtered_connections_.end())
        {
            return false;
        }

        return connection->has_reached_post_device_init;
    }

    void SupportedConnectionSet::SetApiFilter(const std::vector<Api>& filter, ConnectionUpdateHandler* handler)
    {
        const std::lock_guard connection_lock(connection_mutex_);
        api_filter_ = filter;

        UpdateConnections(unfiltered_connections_, handler);
    }

}  // namespace devtrace
