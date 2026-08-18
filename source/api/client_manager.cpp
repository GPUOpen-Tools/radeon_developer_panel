// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for client manager.

#include "client_manager.h"

#include <cstdlib>
#include <cstring>
#include <string>

#ifdef WIN32
#include <Windows.h>
#include <processthreadsapi.h>
#else
#include <unistd.h>
#endif

#include <dd_connection_api.h>
#include <dd_router_utils_api.h>

#include "api_blocklist.h"

namespace
{
    void* StdRealloc([[maybe_unused]] DDAllocatorInstance* instance, void* memory, [[maybe_unused]] size_t old_size, size_t new_size)
    {
        return std::realloc(memory, new_size);
    }

    void StdFree([[maybe_unused]] DDAllocatorInstance* instance, void* memory, [[maybe_unused]] size_t size)
    {
        std::free(memory);
    }

    constexpr DDAllocator kDDAllocator = {nullptr, StdRealloc, StdFree};
}  // namespace

bool ClientManager::ShouldClientBeIgnored(void* userdata, const DDConnectionInfo* connection_info)
{
    ClientManager* manager = reinterpret_cast<ClientManager*>(userdata);
    return manager->ShouldClientBeIgnored(*connection_info);
}

void ClientManager::OnDriverConnected(DDConnectionCallbacksImpl* userdata, const DDConnectionInfo* connection_info)
{
    ClientManager* manager = reinterpret_cast<ClientManager*>(userdata);
    manager->OnDriverConnected(*connection_info);
}

void ClientManager::OnDriverDisconnected(DDConnectionCallbacksImpl* userdata, DDConnectionId umd_connection_id)
{
    ClientManager* manager = reinterpret_cast<ClientManager*>(userdata);
    manager->OnDriverDisconnected(umd_connection_id);
}

ClientManager::ClientManager(DDConnectionApi* api, DDRouterUtilsApi* router_utils, RdpCaptureAppFilter filter, ApiBlocklist* blocklist)
    : api_(api)
    , router_utils_(router_utils)
    , filter_(filter)
    , blocklist_(blocklist)
    , current_process_()
{
    current_process_.process_id = 0;

    DDConnectionFilter connection_filter{};
    connection_filter.pUserData = this;
    connection_filter.filter    = &ClientManager::ShouldClientBeIgnored;
    api_->SetConnectionFilter(api_->pInstance, connection_filter);

    DDConnectionCallbacks connection_callbacks{};
    connection_callbacks.pImpl                = reinterpret_cast<DDConnectionCallbacksImpl*>(this);
    connection_callbacks.OnDriverConnected    = &ClientManager::OnDriverConnected;
    connection_callbacks.OnDriverDisconnected = &ClientManager::OnDriverDisconnected;
    connection_callbacks.OnDriverStateChanged = nullptr;
    connection_callbacks.OnRouterConnected    = nullptr;
    connection_callbacks.OnRouterDisconnected = nullptr;

    api_->AddConnectionCallbacks(api_->pInstance, &connection_callbacks);
}

ClientManager::~ClientManager()
{
    const std::lock_guard lock(connection_mutex_);

    DDConnectionFilter null_filter{};
    null_filter.pUserData = nullptr;
    null_filter.filter    = nullptr;
    api_->SetConnectionFilter(api_->pInstance, null_filter);

    api_->RemoveConnectionCallbacks(api_->pInstance, reinterpret_cast<DDConnectionCallbacksImpl*>(this));
}

void ClientManager::OnDriverConnected(const DDConnectionInfo& connection_info)
{
    const std::lock_guard lock(connection_mutex_);
    if (current_process_.process_id == 0)
    {
        current_process_ = Convert(connection_info);
    }
    else
    {
        DEV_TRACE_ASSERT(current_process_.process_id == connection_info.processId);
    }

    current_umd_connections_.insert(connection_info.umdConnectionId);
}

void ClientManager::OnDriverDisconnected(DDConnectionId umd_connection_id)
{
    const std::lock_guard lock(connection_mutex_);
    current_umd_connections_.erase(umd_connection_id);

    if (current_umd_connections_.empty())
    {
        current_process_.process_id = 0;
        memset(current_process_.process_path, '\0', sizeof(current_process_.process_path));
    }
}

bool ClientManager::ShouldClientBeIgnored(const DDConnectionInfo& connection_info)
{
    RdpCaptureProcessInfo process_info{};
    bool                  blocked = false;

    {
        const std::lock_guard lock(connection_mutex_);
        if (current_process_.process_id != 0 && current_process_.process_id != connection_info.processId)
        {
            return true;
        }

        process_info = Convert(connection_info);

        if (blocklist_ != nullptr && blocklist_->Contains(process_info.process_path))
        {
            blocked = true;
        }
        else if (filter_.filter != nullptr)
        {
            devtrace::Api    raw_api = devtrace::GetApiFromDriverDescription(connection_info.pDescription);
            RdpCaptureGpuApi api     = GetGpuApi(raw_api);

            return !filter_.filter(filter_.user_data, &process_info, api);
        }
        else
        {
            return GetCurrentProcessPid() != connection_info.processId;
        }
    }

    if (blocked)
    {
        blocklist_->NotifyBlocked(process_info.process_path, connection_info.processId);
        return true;
    }

    return false;
}

RdpCaptureProcessInfo ClientManager::Convert(const DDConnectionInfo& connection_info)
{
    RdpCaptureProcessInfo process_info{};
    process_info.process_id = connection_info.processId;

    char*  raw_path  = nullptr;
    size_t path_size = 0;
    if (router_utils_->QueryPathByProcessId(router_utils_->pInstance, connection_info.processId, kDDAllocator, &raw_path, &path_size) != DD_RESULT_SUCCESS ||
        raw_path == nullptr)
    {
        strncpy(process_info.process_path, "Unknown", sizeof(process_info.process_path) - 1);
        process_info.process_path[sizeof(process_info.process_path) - 1] = '\0';
        return process_info;
    }

    strncpy(process_info.process_path, raw_path, sizeof(process_info.process_path) - 1);
    process_info.process_path[sizeof(process_info.process_path) - 1] = '\0';
    kDDAllocator.Free(kDDAllocator.pInstance, raw_path, path_size);
    return process_info;
}

uint32_t ClientManager::GetCurrentProcessPid()
{
#ifdef WIN32
    return static_cast<uint32_t>(GetCurrentProcessId());
#else
    return static_cast<uint32_t>(getpid());
#endif
}

const RdpCaptureProcessInfo& ClientManager::GetProcessInfo()
{
    const std::lock_guard lock(connection_mutex_);
    return current_process_;
}

std::unique_lock<std::recursive_mutex> ClientManager::LockConnections()
{
    return std::unique_lock(connection_mutex_);
}
