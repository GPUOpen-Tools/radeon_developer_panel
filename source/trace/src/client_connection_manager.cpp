// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the object responsible for managing client connections.

#include "client_connection_manager.h"

#include <algorithm>

#include <dd_connection_api.h>
#include <dd_modules_api.h>

#include "client_connections.h"
#include "dev_trace_common.h"

namespace devtrace
{
    ClientConnectionManager::ClientConnectionManager(DDConnectionApi* connection_api)
        : connection_api_(connection_api)
    {
    }

    bool ClientConnectionManager::Initialize(const std::shared_ptr<ClientConnectionSubscriber>& subscriber)
    {
        const std::lock_guard connection_lock(connection_mutex_);
        if (connection_api_ == nullptr)
        {
            return false;
        }

        DDConnectionCallbacks connection_callbacks{};
        connection_callbacks.pImpl                = reinterpret_cast<DDConnectionCallbacksImpl*>(this);
        connection_callbacks.OnDriverConnected    = &ClientConnectionManager::OnDriverConnected;
        connection_callbacks.OnDriverDisconnected = &ClientConnectionManager::OnDriverDisconnected;
        connection_callbacks.OnDriverStateChanged = &ClientConnectionManager::OnDriverStateChanged;
        connection_callbacks.OnRouterConnected    = nullptr;
        connection_callbacks.OnRouterDisconnected = nullptr;

        const DD_RESULT result = connection_api_->AddConnectionCallbacks(connection_api_->pInstance, &connection_callbacks);
        DEV_TRACE_ASSERT(result == DD_RESULT_SUCCESS);

        subscriber_ = subscriber;
        DEV_TRACE_ASSERT(subscriber_->GetConnectingDriverState() <= DD_DRIVER_STATE_POSTDEVICEINIT);

        return true;
    }

    ClientConnectionManager::~ClientConnectionManager()
    {
        const std::lock_guard connection_lock(connection_mutex_);
        if (connection_api_ == nullptr)
        {
            return;
        }

        connection_api_->RemoveConnectionCallbacks(connection_api_->pInstance, reinterpret_cast<DDConnectionCallbacksImpl*>(this));
    }

    void ClientConnectionManager::SetEnabled(bool enabled)
    {
        const std::lock_guard connection_lock(connection_mutex_);
        DEV_TRACE_ASSERT(current_connections_.empty());

        is_enabled_ = enabled;
    }

    void ClientConnectionManager::OnDriverConnected(struct DDConnectionCallbacksImpl* userdata, const DDConnectionInfo* connection_info)
    {
        ClientConnectionManager* manager = reinterpret_cast<ClientConnectionManager*>(userdata);
        manager->OnDriverConnected(*connection_info);
    }

    void ClientConnectionManager::OnDriverDisconnected(DDConnectionCallbacksImpl* userdata, DDConnectionId umd_connection_id)
    {
        ClientConnectionManager* manager = reinterpret_cast<ClientConnectionManager*>(userdata);
        manager->OnDriverDisconnected(umd_connection_id);
    }

    void ClientConnectionManager::OnDriverStateChanged(DDConnectionCallbacksImpl* userdata, DDConnectionId umd_connection_id, DD_DRIVER_STATE state)
    {
        ClientConnectionManager* manager = reinterpret_cast<ClientConnectionManager*>(userdata);
        manager->OnDriverStateChanged(umd_connection_id, state);
    }

    void ClientConnectionManager::OnDriverConnected(const DDConnectionInfo& connection_info)
    {
        const std::lock_guard connection_lock(connection_mutex_);
        if (!is_enabled_)
        {
            return;
        }

        if (HasExistingConnection(connection_info.umdConnectionId))
        {
            return;
        }

        pending_connections_.push_back({GetApiFromDriverDescription(connection_info.pDescription),
                                        connection_info.umdConnectionId,
                                        connection_info.processId,
                                        connection_info.pProcessName});
    }

    void ClientConnectionManager::OnDriverDisconnected(DDConnectionId umd_connection_id)
    {
        // We don't check if the manager is enabled here so that a connection doesn't get stuck connected
        const std::lock_guard connection_lock(connection_mutex_);

        auto existing_pending    = FindConnection(pending_connections_, umd_connection_id);
        auto existing_connection = FindConnection(current_connections_, umd_connection_id);

        if (existing_pending != pending_connections_.end())
        {
            pending_connections_.erase(existing_pending);
        }

        if (existing_connection != current_connections_.end())
        {
            current_connections_.erase(existing_connection);
        }

        subscriber_->UpdateClientConnections(current_connections_);
    }

    void ClientConnectionManager::OnDriverStateChanged(DDConnectionId umd_connection_id, DD_DRIVER_STATE state)
    {
        const std::lock_guard connection_lock(connection_mutex_);

        bool should_update_subscriber = false;
        if (state == subscriber_->GetConnectingDriverState())
        {
            auto pending_connection = FindConnection(pending_connections_, umd_connection_id);
            if (pending_connection != pending_connections_.end())
            {
                current_connections_.push_back(*pending_connection);
                pending_connections_.erase(pending_connection);

                should_update_subscriber = true;
            }
        }

        if (state == DD_DRIVER_STATE_POSTDEVICEINIT)
        {
            auto existing_connection = FindConnection(current_connections_, umd_connection_id);
            if (existing_connection != current_connections_.end())
            {
                existing_connection->has_reached_post_device_init = true;
                should_update_subscriber                          = true;
            }
        }

        if (should_update_subscriber)
        {
            subscriber_->UpdateClientConnections(current_connections_);
        }
    }

    bool ClientConnectionManager::HasExistingConnection(DDConnectionId umd_connection_id)
    {
        const auto pending_connection = FindConnection(pending_connections_, umd_connection_id);
        if (pending_connection != pending_connections_.end())
        {
            return true;
        }

        const auto current_connection = FindConnection(current_connections_, umd_connection_id);
        return current_connection != current_connections_.end();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    ClientConnectionManagerV2::ClientConnectionManagerV2(DDConnectionApi* connection_api)
        : connection_api_(connection_api)
    {
    }

    bool ClientConnectionManagerV2::Initialize(const std::shared_ptr<ClientConnectionSubscriberV2>& subscriber)
    {
        if (connection_api_ == nullptr)
        {
            return false;
        }

        DDConnectionCallbacks connection_callbacks{};
        connection_callbacks.pImpl                = reinterpret_cast<DDConnectionCallbacksImpl*>(this);
        connection_callbacks.OnDriverConnected    = &ClientConnectionManagerV2::OnDriverConnected;
        connection_callbacks.OnDriverDisconnected = &ClientConnectionManagerV2::OnDriverDisconnected;
        connection_callbacks.OnDriverStateChanged = &ClientConnectionManagerV2::OnDriverStateChanged;
        connection_callbacks.OnRouterConnected    = &ClientConnectionManagerV2::OnRouterConnected;
        ;
        connection_callbacks.OnRouterDisconnected = &ClientConnectionManagerV2::OnRouterDisconnected;
        ;

        const DD_RESULT result = connection_api_->AddConnectionCallbacks(connection_api_->pInstance, &connection_callbacks);
        DEV_TRACE_ASSERT(result == DD_RESULT_SUCCESS);

        subscriber_ = subscriber;

        return true;
    }

    ClientConnectionManagerV2::~ClientConnectionManagerV2()
    {
        if (connection_api_ == nullptr)
        {
            return;
        }

        connection_api_->RemoveConnectionCallbacks(connection_api_->pInstance, reinterpret_cast<DDConnectionCallbacksImpl*>(this));
    }

    void ClientConnectionManagerV2::SetEnabled(bool enabled)
    {
        is_enabled_ = enabled;
    }

    void ClientConnectionManagerV2::OnDriverConnected(DDConnectionCallbacksImpl* userdata, const DDConnectionInfo* connection_info)
    {
        ClientConnectionManagerV2* manager = reinterpret_cast<ClientConnectionManagerV2*>(userdata);
        if (manager->is_enabled_)
        {
            manager->subscriber_->OnDriverConnected(*connection_info);
        }
    }

    void ClientConnectionManagerV2::OnDriverDisconnected(DDConnectionCallbacksImpl* userdata, DDConnectionId umd_connection_id)
    {
        ClientConnectionManagerV2* manager = reinterpret_cast<ClientConnectionManagerV2*>(userdata);
        manager->subscriber_->OnDriverDisconnected(umd_connection_id);
    }

    void ClientConnectionManagerV2::OnDriverStateChanged(DDConnectionCallbacksImpl* userdata, DDConnectionId umd_connection_id, DD_DRIVER_STATE state)
    {
        ClientConnectionManagerV2* manager = reinterpret_cast<ClientConnectionManagerV2*>(userdata);
        if (manager->is_enabled_)
        {
            manager->subscriber_->OnDriverStateChanged(umd_connection_id, state);
        }
    }

    void ClientConnectionManagerV2::OnRouterConnected(DDConnectionCallbacksImpl* userdata, [[maybe_unused]] DDConnectionId connection_id)
    {
        ClientConnectionManagerV2* manager = reinterpret_cast<ClientConnectionManagerV2*>(userdata);
        manager->subscriber_->RouterConnectionStatusChanged(true);
    }

    void ClientConnectionManagerV2::OnRouterDisconnected(DDConnectionCallbacksImpl* userdata)
    {
        ClientConnectionManagerV2* manager = reinterpret_cast<ClientConnectionManagerV2*>(userdata);
        manager->subscriber_->RouterConnectionStatusChanged(false);
    }

}  // namespace devtrace
