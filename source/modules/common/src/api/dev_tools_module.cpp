// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  The implementation for the shared DevTools module API and interfaces.

#include "api/dev_tools_module.h"

#include <dd_connection_api.h>
#include <dd_driver_utils_api.h>
#include <dd_modules_api.h>
#include <dd_router_utils_api.h>

#include <QtGlobal>

#include <client_connection_manager.h>
#include <trace_source.h>

#include "api/dev_tools_logging.h"
#include "model/trace_source_view_model.h"
#include "model/utility/userdata_view_model.h"

DD_RESULT DevToolsModule::InitializeModule(DDModuleInstance* module_instance_ptr)
{
    DevToolsModule* module_instance = reinterpret_cast<DevToolsModule*>(module_instance_ptr);
    return module_instance->InitializeAndRegister();
}

void DevToolsModule::DestroyModule(DDModuleInstance* module_instance_ptr)
{
    DevToolsModule* module_instance = reinterpret_cast<DevToolsModule*>(module_instance_ptr);
    module_instance->UnregisterAndDestroy();

    delete module_instance;
}

void DevToolsModule::OnRouterConnected(DDConnectionCallbacksImpl* instance, uint16_t connection_id)
{
    Q_UNUSED(connection_id);

    DevToolsModule* module_instance = reinterpret_cast<DevToolsModule*>(instance);
    module_instance->HandleOnRouterConnected();
}

void DevToolsModule::HandleOnRouterConnected()
{
}

void DevToolsModule::OnRouterDisconnected(DDConnectionCallbacksImpl* instance)
{
    DevToolsModule* module_instance = reinterpret_cast<DevToolsModule*>(instance);
    module_instance->HandleOnRouterDisconnected();
}

void DevToolsModule::HandleOnRouterDisconnected()
{
}

DevToolsModule::DevToolsModule(struct DDApiRegistry* api_registry, const std::string& name, const std::string& display_name)
    : api_registry_(api_registry)
    , name_(name)
    , display_name_(display_name)
{
    DDModulesApi*   modules_api = nullptr;
    const DD_RESULT result      = api_registry_->Get(api_registry_->pInstance,
                                                DD_MODULES_API_NAME,
                                                DDVersion{DD_MODULES_API_VERSION_MAJOR, DD_MODULES_API_VERSION_MINOR, DD_MODULES_API_VERSION_PATCH},
                                                reinterpret_cast<void**>(&modules_api));

    Q_ASSERT(result == DD_RESULT_SUCCESS);

    module_callbacks_ = new DDModulesCallbacks{reinterpret_cast<DDModuleInstance*>(this), &DevToolsModule::InitializeModule, &DevToolsModule::DestroyModule};
    modules_api->AddModulesCallbacks(modules_api->pInstance, module_callbacks_);
}

DevToolsModule::~DevToolsModule()
{
    delete module_callbacks_;
}

DD_RESULT DevToolsModule::InitializeAndRegister()
{
    if (has_initialized_)
    {
        return DD_RESULT_UNKNOWN;
    }

    DD_RESULT result = LoadApis();
    if (result != DD_RESULT_SUCCESS)
    {
        return result;
    }

    result = RegisterConnectionCallbacks();
    if (result != DD_RESULT_SUCCESS)
    {
        return result;
    }

    ubertrace_factory_     = devtrace_deps_api_->get_ubertrace_factory(dev_tools_module_api_->instance);
    overlay_manager_       = devtrace_deps_api_->get_overlay_manager(dev_tools_module_api_->instance);
    device_clocks_manager_ = devtrace_deps_api_->get_device_clocks_manager(dev_tools_module_api_->instance);
    connection_manager_v2_ = std::make_unique<devtrace::ClientConnectionManagerV2>(connection_api_);

    class QSettings* tool_settings = dev_tools_module_api_->get_tool_settings(dev_tools_module_api_->instance);
    result                         = Initialize(tool_settings);

    if (result != DD_RESULT_SUCCESS)
    {
        return result;
    }

    dev_tools_module_api_->register_module(dev_tools_module_api_->instance, this);
    has_initialized_ = true;

    return DD_RESULT_SUCCESS;
}

DD_RESULT DevToolsModule::LoadApis()
{
    DD_RESULT result = api_registry_->Get(api_registry_->pInstance,
                                          kDevToolsModuleApiName,
                                          DDVersion{kDevToolsModuleApiVersionMajor, kDevToolsModuleApiVersionMinor, kDevToolsModuleApiVersionPatch},
                                          reinterpret_cast<void**>(&dev_tools_module_api_));

    if (result != DD_RESULT_SUCCESS)
    {
        return result;
    }

    result = api_registry_->Get(api_registry_->pInstance,
                                kDevTraceDependenciesApiName,
                                DDVersion{kDevTraceDependenciesApiMajor, kDevTraceDependenciesApiMinor, kDevTraceDependenciesApiPatch},
                                reinterpret_cast<void**>(&devtrace_deps_api_));

    if (result != DD_RESULT_SUCCESS)
    {
        return result;
    }

    result = api_registry_->Get(api_registry_->pInstance,
                                DD_ROUTER_UTILS_API_NAME,
                                DDVersion{DD_ROUTER_UTILS_API_VERSION_MAJOR, DD_ROUTER_UTILS_API_VERSION_MINOR, DD_ROUTER_UTILS_API_VERSION_PATCH},
                                reinterpret_cast<void**>(&router_utils_api_));

    if (result != DD_RESULT_SUCCESS)
    {
        return result;
    }

    result = api_registry_->Get(api_registry_->pInstance,
                                DD_DRIVER_UTILS_API_NAME,
                                DDVersion{DD_DRIVER_UTILS_API_VERSION_MAJOR, DD_DRIVER_UTILS_API_VERSION_MINOR, DD_DRIVER_UTILS_API_VERSION_PATCH},
                                reinterpret_cast<void**>(&driver_utils_api_));

    if (result != DD_RESULT_SUCCESS)
    {
        return result;
    }

    result = api_registry_->Get(api_registry_->pInstance,
                                DD_CONNECTION_API_NAME,
                                DDVersion{DD_CONNECTION_API_VERSION_MAJOR, DD_CONNECTION_API_VERSION_MINOR, DD_CONNECTION_API_VERSION_PATCH},
                                reinterpret_cast<void**>(&connection_api_));

    if (result != DD_RESULT_SUCCESS)
    {
        return result;
    }

    return api_registry_->Get(api_registry_->pInstance,
                              kDevToolsLoggingApiName,
                              DDVersion{kDevToolsLoggingApiVersionMajor, kDevToolsLoggingApiVersionMinor, kDevToolsLoggingApiVersionPatch},
                              reinterpret_cast<void**>(&logging_api_));
}

DD_RESULT DevToolsModule::RegisterConnectionCallbacks()
{
    DDConnectionCallbacks connection_callbacks{};
    connection_callbacks.pImpl                = reinterpret_cast<DDConnectionCallbacksImpl*>(this);
    connection_callbacks.OnRouterConnected    = &DevToolsModule::OnRouterConnected;
    connection_callbacks.OnRouterDisconnected = &DevToolsModule::OnRouterDisconnected;
    connection_callbacks.OnDriverConnected    = nullptr;
    connection_callbacks.OnDriverDisconnected = nullptr;

    return connection_api_->AddConnectionCallbacks(connection_api_->pInstance, &connection_callbacks);
}

void DevToolsModule::UnregisterAndDestroy()
{
    if (!has_initialized_)
    {
        return;
    }

    ubertrace_factory_ = nullptr;

    const DD_RESULT result = connection_api_->RemoveConnectionCallbacks(connection_api_->pInstance, reinterpret_cast<DDConnectionCallbacksImpl*>(this));
    Q_ASSERT(result == DD_RESULT_SUCCESS);

    Q_ASSERT(dev_tools_module_api_ != nullptr);
    dev_tools_module_api_->unregister_module(dev_tools_module_api_->instance, this);

    Destroy();

    has_initialized_ = false;
}

const std::string& DevToolsModule::GetModuleName() const
{
    return name_;
}

const std::string& DevToolsModule::GetModuleDisplayName() const
{
    return display_name_;
}

ModuleCategory DevToolsModule::GetModuleCategory() const
{
    return ModuleCategory::kCapture;
}

void DevToolsModule::ResetToDefaults()
{
    if (base_userdata_view_model_ == nullptr)
    {
        return;
    }

    base_userdata_view_model_->InitializeDefaultsAndApply();
}

void DevToolsModule::SerializeData(const std::string& serialized_data)
{
    dev_tools_module_api_->serialize_module(dev_tools_module_api_->instance, this, serialized_data);

    if (base_trace_source_view_model_ != nullptr)
    {
        const bool parse_result = base_trace_source_view_model_->ReceiveUserData(serialized_data);
        Q_ASSERT(parse_result);
    }

    if (base_userdata_view_model_ != nullptr)
    {
        const bool result = base_userdata_view_model_->ReceiveUserData(serialized_data);
        Q_ASSERT(result);
    }
}

bool DevToolsModule::LoadSerializedData(const std::string& data)
{
    if (base_trace_source_view_model_ != nullptr)
    {
        if (!base_trace_source_view_model_->ReceiveUserData(data))
        {
            return false;
        }
    }

    if (base_userdata_view_model_ != nullptr)
    {
        if (!base_userdata_view_model_->ReceiveUserData(data))
        {
            return false;
        }
    }

    return base_userdata_view_model_->LoadSerializedData(data);
}

bool DevToolsModule::IsCompatibleWithApi(devtrace::Api api) const
{
    Q_UNUSED(api);
    return false;
}

bool DevToolsModule::IsCompatibleWithModuleNamed(const std::string& module_name) const
{
    Q_UNUSED(module_name);
    return true;
}

bool DevToolsModule::SetEnabled(bool is_enabled)
{
    const std::lock_guard<std::mutex> enabled_lock(enabled_mutex_);

    if (is_enabled_ == is_enabled)
    {
        return true;
    }

    connection_manager_v2_->SetEnabled(is_enabled);

    if (is_enabled)
    {
        is_enabled_ = Enable();
        return is_enabled_;
    }

    is_enabled_ = !Disable();
    return !is_enabled_;
}

bool DevToolsModule::IsEnabled() const
{
    return is_enabled_;
}

bool DevToolsModule::NeedsDriverConnections() const
{
    return IsEnabled();
}

bool DevToolsModule::Enable()
{
    return true;
}

bool DevToolsModule::Disable()
{
    return true;
}

void DevToolsModule::SetDisplayApplication(const QString& application_name)
{
    if (base_trace_source_view_model_ == nullptr)
    {
        return;
    }

    base_trace_source_view_model_->SetDisplayApplicationName(application_name);
}
