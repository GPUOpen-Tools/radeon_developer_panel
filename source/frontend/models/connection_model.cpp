// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for class that manages tool connections.

#include "connection_model.h"

#include <optional>
#include <thread>

#include <QtConcurrent/QtConcurrent>

#include <ddCommon.h>
#include <dd_logger_api.h>
#include <dd_tool_api.h>

#include <g_RouterUtilsModuleInterface.h>
#include <g_SiphonModuleInterface.h>
#include <g_SystemTraceModuleStatic.h>

#include "logging/logging_manager.h"
#include "settings_manager.h"
#include "timeout_model.h"

namespace rdp
{
    void ConnectionModel::OnRouterConnected(DDConnectionCallbacksImpl* instance, [[maybe_unused]] const DDConnectionId connection_id)
    {
        const auto model = reinterpret_cast<ConnectionModel*>(instance);

        const std::lock_guard connection_lock(model->connection_mutex_);
        Q_ASSERT(model->connection_state_ == ConnectionState::kConnecting);
        model->connection_state_ = ConnectionState::kConnected;

        emit model->NetConnected(model->connection_description_, model->currently_reconnecting_);
        model->currently_reconnecting_ = false;
    }

    void ConnectionModel::OnRouterDisconnected(DDConnectionCallbacksImpl* instance)
    {
        const auto model = reinterpret_cast<ConnectionModel*>(instance);
        model->OnRouterDisconnected();
    }

    ConnectionModel::ConnectionModel(const std::shared_ptr<ModuleModel>&     module_model,
                                     const std::shared_ptr<SettingsManager>& settings_manager,
                                     const std::shared_ptr<TimeoutModel>&    timeout_model)
        : module_model_(module_model)
        , settings_manager_(settings_manager)
        , timeout_model_(timeout_model)
        , should_reconnect_(false)
        , currently_reconnecting_(false)
    {
    }

    void ConnectionModel::ShutDown()
    {
        should_reconnect_ = false;
        SyncDisconnectTool();

        if (connection_thread_.joinable())
        {
            connection_thread_.join();
        }

        if (tool_api_ != nullptr)
        {
            Q_ASSERT(connection_api_ != nullptr);
            connection_api_->RemoveConnectionCallbacks(connection_api_->pInstance, reinterpret_cast<DDConnectionCallbacksImpl*>(this));

            module_model_->OnToolBeingDestroyed();
            DDToolApiDestroy(&tool_api_);
        }
    }

    // Creates a new DDTool context
    bool ConnectionModel::CreateTool()
    {
        if (tool_api_ != nullptr)
        {
            return true;
        }

        CustomDDTimeouts dd_timeouts{};

        constexpr char desc[] = "Radeon Developer Panel";

        const auto create_info = DDToolApiCreateInfo{&desc[0],
                                                     sizeof(desc) - 1,
                                                     nullptr,
                                                     0,
                                                     nullptr,
                                                     0,
                                                     LoggingManager::GetLoggerCallback(),
                                                     static_cast<uint32_t>(dd_timeouts.retry_timeout_ms),
                                                     static_cast<uint32_t>(dd_timeouts.communication_timeout_ms),
                                                     static_cast<uint32_t>(dd_timeouts.connection_timeout_ms)};

        if (DDToolApiCreate(&create_info, &tool_api_) != DD_RESULT_SUCCESS)
        {
            return false;
        }

        DDApiRegistry* api_registry = tool_api_->GetApiRegistry(tool_api_->pInstance);
        LoggingManager::Register(api_registry);

        if (api_registry->Get(api_registry->pInstance,
                              DD_CONNECTION_API_NAME,
                              DDVersion{DD_CONNECTION_API_VERSION_MAJOR, DD_CONNECTION_API_VERSION_MINOR, DD_CONNECTION_API_VERSION_PATCH},
                              reinterpret_cast<void**>(&connection_api_)) != DD_RESULT_SUCCESS)
        {
            DDToolApiDestroy(&tool_api_);
            return false;
        }

        // Register connection callbacks
        DDConnectionCallbacks router_connection_callbacks{};
        router_connection_callbacks.pImpl                = reinterpret_cast<DDConnectionCallbacksImpl*>(this);
        router_connection_callbacks.OnRouterConnected    = &ConnectionModel::OnRouterConnected;
        router_connection_callbacks.OnRouterDisconnected = &ConnectionModel::OnRouterDisconnected;
        router_connection_callbacks.OnDriverConnected    = nullptr;
        router_connection_callbacks.OnDriverDisconnected = nullptr;
        router_connection_callbacks.OnDriverStateChanged = nullptr;

        if (connection_api_->AddConnectionCallbacks(connection_api_->pInstance, &router_connection_callbacks) != DD_RESULT_SUCCESS)
        {
            DDToolApiDestroy(&tool_api_);
            return false;
        }

        return true;
    }

    void ConnectionModel::OnBind()
    {
        const std::lock_guard connection_lock(connection_mutex_);
        Q_ASSERT(connection_state_ == ConnectionState::kDisconnected);

        emit NetDisconnected();
    }

    ConnectionType ConnectionModel::GetActiveConnectionType() const
    {
        return active_connection_type_;
    }

    DDApiRegistry* ConnectionModel::GetApiRegistry() const
    {
        if (tool_api_ != nullptr)
        {
            return tool_api_->GetApiRegistry(tool_api_->pInstance);
        }

        return nullptr;
    }

    DD_RESULT ConnectionModel::CreateRouter()
    {
        const std::lock_guard router_lock(router_mutex_);
        if (router_ != DD_API_INVALID_HANDLE)
        {
            return DD_RESULT_SUCCESS;
        }

        DDRouterCreateInfo router_create_info{};
        router_create_info.pDescription = "Radeon Developer Tool Router";
        router_create_info.alloc        = {ddApiDefaultAlloc, ddApiDefaultFree, nullptr};
        router_create_info.logger       = {};

        if (disable_client_timeout_)
        {
            router_create_info.clientTimeoutCount = static_cast<uint32_t>(-1);
        }

        if (const DD_RESULT create_result = ddRouterCreate(&router_create_info, &router_); create_result != DD_RESULT_SUCCESS)
        {
            return create_result;
        }

        for (const std::vector        modules = {SystemTraceQueryModule(), RouterUtilsQueryModuleInterface(), SiphonQueryModuleInterface()};
             const DDModuleInterface* module : modules)
        {
            DDModuleLoadedInfo module_loaded_info;

            if (const DD_RESULT result = ddRouterLoadBuiltinModule(router_, module, &module_loaded_info); result != DD_RESULT_SUCCESS)
            {
                ddRouterDestroy(router_);
                router_ = DD_API_INVALID_HANDLE;

                RDP_LOG_ERROR("Failed to load devtools router module");
                return result;
            }
        }

        return DD_RESULT_SUCCESS;
    }

    void ConnectionModel::DestroyRouter()
    {
        const std::lock_guard router_lock(router_mutex_);
        if (router_ == DD_API_INVALID_HANDLE)
        {
            return;
        }

        ddRouterDestroy(router_);
        router_ = DD_API_INVALID_HANDLE;
    }

    bool ConnectionModel::LoadModules() const
    {
        if (tool_api_ == nullptr)
        {
            return false;
        }

        DDApiRegistry* api_registry = tool_api_->GetApiRegistry(tool_api_->pInstance);
        return module_model_->Load(api_registry, tool_api_).has_value();
    }

    void ConnectionModel::ConnectToolLocal(const bool use_existing_local_router)
    {
        ConnectToolAsync(nullptr, 0, "Local", ConnectionType::kLocal, use_existing_local_router);
    }

    void ConnectionModel::ConnectToolRemote(const RemoteConnectionInfo& connection_info)
    {
        const std::string hostname = connection_info.hostname.toStdString();
        ConnectToolAsync(hostname.c_str(), static_cast<uint32_t>(connection_info.port), connection_info.GetDescription(), ConnectionType::kRemote);
    }

    void ConnectionModel::ConnectToolAsync(const char*    ip_str,
                                           uint32_t       port,
                                           const QString& description,
                                           ConnectionType connection_type,
                                           bool           do_not_create_router)
    {
        if (connection_thread_.joinable())
        {
            connection_thread_.join();
        }

        // The memory for the ip string will be freed by the time ConnectTool is executed, so we copy it into an optional string to avoid
        // manual memory management.
        std::optional<std::string> captured_ip;
        if (ip_str != nullptr)
        {
            captured_ip = ip_str;
        }

        connection_thread_ = std::thread([this, captured_ip, port, description, connection_type, do_not_create_router] {
            const char* final_ip_str = captured_ip.has_value() ? captured_ip->c_str() : nullptr;
            ConnectTool(final_ip_str, port, description, connection_type, do_not_create_router);
        });
    }

    void ConnectionModel::ConnectTool(const char*          ip_str,
                                      const uint32_t       port,
                                      const QString&       description,
                                      const ConnectionType connection_type,
                                      const bool           do_not_create_router)
    {
        // This value is changed if and only if we are connecting to a local connection and do_not_create_router is false, meaning that for any connection
        // to actually proceed this should be DD_RESULT_SUCCESS.
        DD_RESULT router_create_result = DD_RESULT_SUCCESS;

        {
            const std::lock_guard connection_lock(connection_mutex_);

            if (tool_api_ == nullptr || connection_state_ != ConnectionState::kDisconnected)
            {
                currently_reconnecting_ = false;
                return;
            }

            // For local connections, we need to create a router so there is something to connect to (unless the do_not_create_router flag is specified)
            if (connection_type == ConnectionType::kLocal && !do_not_create_router)
            {
                router_create_result = CreateRouter();
            }

            if (router_create_result == DD_RESULT_SUCCESS)
            {
                connection_state_       = ConnectionState::kConnecting;
                active_connection_type_ = connection_type;
                connection_description_ = description;
            }
        }

        // We act on the router creation result here because we don't want to emit signals while the lock is still held to avoid deadlock.
        if (router_create_result != DD_RESULT_SUCCESS)
        {
            if (router_create_result == DD_RESULT_DD_GENERIC_UNAVAILABLE)
            {
                emit ExistingLocalRouterFound();
            }
            else
            {
                emit RouterCreationFailed();
            }

            currently_reconnecting_ = false;
            return;
        }

        emit NetConnecting(connection_description_);

        if (tool_api_->Connect(tool_api_->pInstance, ip_str, port) != DD_RESULT_SUCCESS)
        {
            {
                const std::lock_guard connection_lock(connection_mutex_);
                connection_state_       = ConnectionState::kDisconnected;
                active_connection_type_ = ConnectionType::kUnknown;
                connection_description_ = "";
            }

            currently_reconnecting_ = false;
            emit NetDisconnected();
        }
    }

    void ConnectionModel::DisconnectTool()
    {
        if (connection_thread_.joinable())
        {
            connection_thread_.join();
        }

        connection_thread_ = std::thread([&] { SyncDisconnectTool(); });
    }

    void ConnectionModel::SyncDisconnectTool()
    {
        {
            const std::lock_guard connection_lock(connection_mutex_);
            if (tool_api_ == nullptr || connection_state_ != ConnectionState::kConnected)
            {
                return;
            }

            connection_state_ = ConnectionState::kDisconnecting;
        }

        emit NetDisconnecting();
        tool_api_->Disconnect(tool_api_->pInstance);

        if (currently_reconnecting_)
        {
            Q_ASSERT(active_connection_type_ == ConnectionType::kLocal);
            ConnectTool(nullptr, 0, connection_description_, ConnectionType::kLocal, false);
        }
    }

    void ConnectionModel::OnRouterDisconnected()
    {
        bool unexpectedly_disconnected = false;

        {
            const std::lock_guard connection_lock(connection_mutex_);

            unexpectedly_disconnected = connection_state_ != ConnectionState::kDisconnecting;
            connection_state_         = ConnectionState::kDisconnected;
            DestroyRouter();

            currently_reconnecting_ = should_reconnect_.exchange(false);

            if (!currently_reconnecting_)
            {
                active_connection_type_ = ConnectionType::kUnknown;
                connection_description_ = "";
            }
        }

        emit NetDisconnected();
        if (unexpectedly_disconnected)
        {
            emit NetDisconnectedUnexpectedly();
        }
    }

    bool ConnectionModel::IsConnected()
    {
        const std::lock_guard connection_lock(connection_mutex_);
        return connection_state_ == ConnectionState::kConnected;
    }

    void ConnectionModel::SetDisableClientTimeout(const bool disable_client_timeout)
    {
        {
            const std::lock_guard router_lock(router_mutex_);
            if (disable_client_timeout_ == disable_client_timeout)
            {
                return;
            }

            disable_client_timeout_ = disable_client_timeout;
        }

        const std::lock_guard connection_lock(connection_mutex_);
        if (connection_state_ == ConnectionState::kConnected)
        {
            should_reconnect_ = true;
            DisconnectTool();
        }
    }

    bool ConnectionModel::CanClientTimeoutBeDisabledForCurrentConnection()
    {
        const std::lock_guard connection_lock(connection_mutex_);
        const std::lock_guard router_lock(router_mutex_);

        return active_connection_type_ == ConnectionType::kLocal && router_ != DD_API_INVALID_HANDLE;
    }

    bool ConnectionModel::GetIsClientTimeoutDisabled()
    {
        const std::lock_guard router_lock(router_mutex_);
        return disable_client_timeout_;
    }

}  // namespace rdp
