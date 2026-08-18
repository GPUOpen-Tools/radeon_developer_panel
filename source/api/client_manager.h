// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for client manager.

#ifndef RDP_SOURCE_API_CAPTURE_CLIENT_MANAGER_H_
#define RDP_SOURCE_API_CAPTURE_CLIENT_MANAGER_H_

#include <atomic>
#include <mutex>
#include <unordered_set>

#include <dd_common_api.h>

#include <dipper.h>

#include "RdpCaptureApi.h"
#include "api_blocklist.h"
#include "api_data.h"

class ClientManager
{
    /// @brief Determines if a client connection should be accepted or not.
    /// @param [in] userdata The model to use to check if the client should be ignored.
    /// @param [in] connection_info The connection info of the client.
    /// @return true if the client should be ignored, false otherwise.
    static bool ShouldClientBeIgnored(void* userdata, const struct DDConnectionInfo* connection_info);

    /// @brief Called when a client connects.
    /// @param [in] userdata The model to notify of the connection.
    /// @param [in] connection_info The connection info of the client.
    static void OnDriverConnected(struct DDConnectionCallbacksImpl* userdata, const struct DDConnectionInfo* connection_info);

    /// @brief Called when a client connects.
    /// @param [in] userdata The model to notify of the connection.
    /// @param [in] umd_connection_id The identifier of the UMD connection.
    static void OnDriverDisconnected(struct DDConnectionCallbacksImpl* userdata, DDConnectionId umd_connection_id);

public:
    /// @brief Constructor.
    /// @param [in] api          The connection API to use.
    /// @param [in] router_utils Utils to use to get the process path.
    /// @param [in] filter       The filter to use.
    /// @param [in] blocklist    The blocklist to check before invoking the filter. May be nullptr.
    DIP(ClientManager(struct DDConnectionApi* api, struct DDRouterUtilsApi* router_utils, struct RdpCaptureAppFilter filter, ApiBlocklist* blocklist));

    /// @brief Destructor.
    ~ClientManager();

private:
    /// @brief Called when a client connects.
    /// @param [in] connection_info The connection info of the client.
    void OnDriverConnected(const DDConnectionInfo& connection_info);

    /// @brief Called when a client connects.
    /// @param [in] umd_connection_id The identifier of the UMD connection.
    void OnDriverDisconnected(DDConnectionId umd_connection_id);

    /// @brief Determines if a client connection should be accepted or not.
    /// @param [in] connection_info The connection info of the client.
    /// @return true if the client should be ignored, false otherwise.
    bool ShouldClientBeIgnored(const DDConnectionInfo& connection_info);

    /// @brief Converts the connection info to process info.
    /// @param [in] connection_info The connection info to convert.
    /// @return The converted connection info.
    RdpCaptureProcessInfo Convert(const DDConnectionInfo& connection_info);

    /// @brief Gets the PID of the currently running process.
    /// @return The PID of the currently running process.
    uint32_t GetCurrentProcessPid();

public:
    /// @brief Gets the current process info.
    /// @return The current process info.
    const RdpCaptureProcessInfo& GetProcessInfo();

    /// @brief Locks the connection handling.
    /// @return The lock that will stop things from connecting / disconnecting.
    std::unique_lock<std::recursive_mutex> LockConnections();

private:
    struct DDConnectionApi*    api_          = nullptr;  ///< The connection API to use.
    struct DDRouterUtilsApi*   router_utils_ = nullptr;  ///< Utils to use to get the process path.
    struct RdpCaptureAppFilter filter_;                  ///< The capture app filter.
    ApiBlocklist*              blocklist_ = nullptr;     ///< The blocklist to check before invoking the user filter.

    std::recursive_mutex               connection_mutex_;         ///< Mutex that serializes connection events.
    RdpCaptureProcessInfo              current_process_;          ///< Info about the currently connected process.
    std::unordered_set<DDConnectionId> current_umd_connections_;  ///< The current UMD connections.
};

#endif
