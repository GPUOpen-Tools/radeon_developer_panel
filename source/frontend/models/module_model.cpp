// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the model that manages all the dev tools modules.

#include "module_model.h"

#include <algorithm>

#include <QtGlobal>

#include <CrashAnalysis.h>
#include <DeviceClocks.h>
#include <MemoryTrace.h>
#include <Profiling.h>
#include <Raytracing.h>

#include "ddCommon.h"

#include <dd_clocks_api.h>
#include <dd_connection_api.h>
#include <dd_driver_utils_api.h>
#include <dd_router_utils_api.h>
#include <dd_tool_api.h>

#include <device_clocks.h>
#include <overlay_manager.h>
#include <system_info_cache.h>
#include <ubertrace_factory.h>

#include "api/dev_tools_module.h"
#include "logging/logging_model.h"
#include "settings_manager.h"

#include <common/inc/model/mercury_logger.h>

namespace rdp
{
    void ModuleModel::RegisterModule(void* instance, DevToolsModule* module)
    {
        auto* model = static_cast<ModuleModel*>(instance);
        model->RegisterModule(module);
    }

    void ModuleModel::UnregisterModule(void* instance, const DevToolsModule* module)
    {
        auto* model = static_cast<ModuleModel*>(instance);
        model->UnregisterModule(module);
    }

    void ModuleModel::ReceiveSerializeModule(void* instance, const DevToolsModule* module, const std::string& serialized_data)
    {
        auto* model = static_cast<ModuleModel*>(instance);
        emit  model->SerializeModule(module, serialized_data);
    }

    QSettings* ModuleModel::GetToolSettings(void* instance)
    {
        const auto* model = static_cast<ModuleModel*>(instance);
        return model->settings_manager_->GetSettings();
    }

    std::shared_ptr<devtrace::UbertraceUserFactory> ModuleModel::GetUbertraceFactory(void* instance)
    {
        auto* model = static_cast<ModuleModel*>(instance);
        return model->ubertrace_factory_;
    }

    std::shared_ptr<devtrace::OverlayManager> ModuleModel::GetOverlayManager(void* instance)
    {
        auto* model = static_cast<ModuleModel*>(instance);
        return model->overlay_manager_;
    }

    std::shared_ptr<devtrace::DeviceClocksManager> ModuleModel::GetDeviceClocksManager(void* instance)
    {
        auto* model = static_cast<ModuleModel*>(instance);
        return model->device_clocks_manager_;
    }

    ModuleModel::ModuleModel(const std::shared_ptr<SettingsManager>& settings_manager, QObject* parent)
        : QAbstractItemModel(parent)
        , settings_manager_(settings_manager)
        , ubertrace_factory_(new devtrace::UbertraceUserFactory())
    {
    }

    tl::expected<void, std::string> ModuleModel::Load(DDApiRegistry* api_registry, const DDToolApi* tool_api)
    {
        DDConnectionApi*  connection_api;
        DDDriverUtilsApi* driver_utils_api;

        DD_RESULT result = api_registry->Get(api_registry->pInstance,
                                             DD_CONNECTION_API_NAME,
                                             DDVersion{DD_CONNECTION_API_VERSION_MAJOR, DD_CONNECTION_API_VERSION_MINOR, DD_CONNECTION_API_VERSION_PATCH},
                                             reinterpret_cast<void**>(&connection_api));

        if (result != DD_RESULT_SUCCESS)
        {
            const auto error = fmt::format("Failed to get DD Connection API: {}", ddApiResultToString(result));
            return tl::unexpected(error);
        }

        result = api_registry->Get(api_registry->pInstance,
                                   DD_DRIVER_UTILS_API_NAME,
                                   DDVersion{DD_DRIVER_UTILS_API_VERSION_MAJOR, DD_DRIVER_UTILS_API_VERSION_MINOR, DD_DRIVER_UTILS_API_VERSION_PATCH},
                                   reinterpret_cast<void**>(&driver_utils_api));

        if (result != DD_RESULT_SUCCESS)
        {
            const auto error = fmt::format("Failed to get DD Driver Utils API: {}", ddApiResultToString(result));
            return tl::unexpected(error);
        }

        DDRouterUtilsApi* router_utils_api;
        result = api_registry->Get(api_registry->pInstance,
                                   DD_ROUTER_UTILS_API_NAME,
                                   DDVersion{DD_ROUTER_UTILS_API_VERSION_MAJOR, DD_ROUTER_UTILS_API_VERSION_MINOR, DD_ROUTER_UTILS_API_VERSION_PATCH},
                                   reinterpret_cast<void**>(&router_utils_api));

        if (result != DD_RESULT_SUCCESS)
        {
            const auto error = fmt::format("Failed to get DD Router Utils API: {}", ddApiResultToString(result));
            return tl::unexpected(error);
        }

        const auto system_info_cache = std::make_shared<devtrace::SystemInfoCache>(router_utils_api);
        if (!CreateOverlayManager(connection_api, driver_utils_api, system_info_cache))
        {
            return tl::unexpected("Failed to create overlay manager.");
        }

        if (!CreateDeviceClocksManager(api_registry, connection_api, system_info_cache))
        {
            return tl::unexpected("Failed to create device clocks manager.");
        }

        if (!RegisterApis(api_registry))
        {
            return tl::unexpected("Failed to register DevToolsModuleApi and DevTraceDependenciesApi.");
        }

        beginResetModel();

        LoadBuiltinModules(api_registry);
        tool_api->LoadModules(tool_api->pInstance);

        endResetModel();

        return {};
    }

    bool ModuleModel::CreateOverlayManager(DDConnectionApi*                                  connection_api,
                                           DDDriverUtilsApi*                                 driver_utils_api,
                                           const std::shared_ptr<devtrace::SystemInfoCache>& sys_info_cache)
    {
        overlay_manager_ = std::make_shared<devtrace::OverlayManager>(driver_utils_api, connection_api, sys_info_cache);
        return overlay_manager_->Initialize();
    }

    bool ModuleModel::CreateDeviceClocksManager(const DDApiRegistry*                              api_registry,
                                                DDConnectionApi*                                  connection_api,
                                                const std::shared_ptr<devtrace::SystemInfoCache>& sys_info_cache)
    {
        DDClocksApi*    clocks_api;
        const DD_RESULT result = api_registry->Get(api_registry->pInstance,
                                                   DD_CLOCKS_API_NAME,
                                                   DDVersion{DD_CLOCKS_API_VERSION_MAJOR, DD_CLOCKS_API_VERSION_MINOR, DD_CLOCKS_API_VERSION_PATCH},
                                                   reinterpret_cast<void**>(&clocks_api));

        if (result != DD_RESULT_SUCCESS)
        {
            return false;
        }

        DevToolsLoggingApi* logging_api = nullptr;
        if (api_registry->Get(api_registry->pInstance,
                              kDevToolsLoggingApiName,
                              DDVersion{kDevToolsLoggingApiVersionMajor, kDevToolsLoggingApiVersionMinor, kDevToolsLoggingApiVersionPatch},
                              reinterpret_cast<void**>(&logging_api)) != DD_RESULT_SUCCESS)
        {
            return false;
        }

        device_clocks_manager_ = std::make_shared<devtrace::DeviceClocksManager>(
            sys_info_cache, overlay_manager_, connection_api, clocks_api, std::make_shared<MercuryLogger>(logging_api));
        return device_clocks_manager_->Initialize();
    }

    bool ModuleModel::RegisterApis(const DDApiRegistry* api_registry)
    {
        DevToolsModuleApi api{};
        api.instance          = this;
        api.register_module   = &ModuleModel::RegisterModule;
        api.unregister_module = &ModuleModel::UnregisterModule;
        api.serialize_module  = &ModuleModel::ReceiveSerializeModule;
        api.get_tool_settings = &ModuleModel::GetToolSettings;

        DD_RESULT result = api_registry->Add(api_registry->pInstance,
                                             kDevToolsModuleApiName,
                                             DDVersion{kDevToolsModuleApiVersionMajor, kDevToolsModuleApiVersionMinor, kDevToolsModuleApiVersionPatch},
                                             &api,
                                             sizeof(DevToolsModuleApi));

        if (result != DD_RESULT_SUCCESS)
        {
            return false;
        }

        DevTraceDependenciesApi devtrace_api{};
        devtrace_api.instance                  = this;
        devtrace_api.get_ubertrace_factory     = &ModuleModel::GetUbertraceFactory;
        devtrace_api.get_overlay_manager       = &ModuleModel::GetOverlayManager;
        devtrace_api.get_device_clocks_manager = &ModuleModel::GetDeviceClocksManager;

        result = api_registry->Add(api_registry->pInstance,
                                   kDevTraceDependenciesApiName,
                                   DDVersion{kDevTraceDependenciesApiMajor, kDevTraceDependenciesApiMinor, kDevTraceDependenciesApiPatch},
                                   &devtrace_api,
                                   sizeof(DevTraceDependenciesApi));

        if (result != DD_RESULT_SUCCESS)
        {
            return false;
        }

        return true;
    }

    void ModuleModel::LoadBuiltinModules(DDApiRegistry* api_registry)
    {
        DDModuleLoad_Profiling(api_registry);
        DDModuleLoad_MemoryTrace(api_registry);
        DDModuleLoad_Raytracing(api_registry);
        DDModuleLoad_CrashAnalysis(api_registry);
        DDModuleLoad_DeviceClocks(api_registry);
    }

    void ModuleModel::RegisterModule(DevToolsModule* module)
    {
        Q_ASSERT(GetModuleIndex(module->GetModuleName()) == -1);
        modules_.push_back(module);
    }

    void ModuleModel::UnregisterModule(const DevToolsModule* module)
    {
        const int module_index = GetModuleIndex(module->GetModuleName());
        if (module_index == -1)
        {
            return;
        }

        modules_.erase(modules_.begin() + module_index);
    }

    void ModuleModel::OnToolBeingDestroyed()
    {
        if (device_clocks_manager_ != nullptr)
        {
            device_clocks_manager_->Unregister();
        }

        if (overlay_manager_ != nullptr)
        {
            overlay_manager_->Unregister();
        }
    }

    int ModuleModel::GetModuleIndex(const std::string& name) const
    {
        const auto result = std::ranges::find_if(modules_, [=](const DevToolsModule* candidate) { return candidate->GetModuleName() == name; });
        return result == modules_.end() ? -1 : static_cast<int>(std::distance(modules_.begin(), result));
    }

    std::vector<const DevToolsModule*> ModuleModel::GetModules() const
    {
        return {modules_.begin(), modules_.end()};
    }

    std::vector<const DevToolsModule*> ModuleModel::GetEnabledModules() const
    {
        std::vector<const DevToolsModule*> enabled_modules;
        for (const DevToolsModule* module : modules_)
        {
            // System modules are always active — they are not gated by the
            // enable/disable workflow and should always count as enabled features.
            if (module->IsEnabled() || module->GetModuleCategory() == ModuleCategory::kSystem)
            {
                enabled_modules.push_back(module);
            }
        }

        return enabled_modules;
    }

    bool ModuleModel::HasModulesNeedingConnections() const
    {
        for (const DevToolsModule* module : modules_)
        {
            if (module->NeedsDriverConnections())
            {
                return true;
            }
        }
        return false;
    }

    void ModuleModel::SetModuleEnabled(const DevToolsModule* module, const bool is_enabled)
    {
        SetModuleEnabled(module->GetModuleName(), is_enabled);
    }

    void ModuleModel::SetModuleEnabled(const std::string& module, const bool is_enabled)
    {
        if (modules_locked_)
        {
            return;
        }

        const int module_index = GetModuleIndex(module);
        if (module_index == -1)
        {
            return;
        }

        DevToolsModule* mutable_module = modules_[module_index];

        // System modules are always active and must not be disabled (e.g. by preset loading).
        if (!is_enabled && mutable_module->GetModuleCategory() == ModuleCategory::kSystem)
        {
            return;
        }

        if (mutable_module->IsEnabled() == is_enabled)
        {
            return;
        }

        if (is_enabled && !IsModuleCompatibleWithEnabledModules(mutable_module))
        {
            return;
        }

        if (mutable_module->SetEnabled(is_enabled))
        {
            emit ModuleStatusChanged(mutable_module, is_enabled);

            const QModelIndex enabled_index = index(module_index, kModuleModelColumnsEnabled, {});
            emit              dataChanged(enabled_index, enabled_index);

            const QModelIndex compatibility_start = index(0, kModuleModelColumnsIncompatible, {});
            const QModelIndex compatibility_end   = index(static_cast<int>(modules_.size()) - 1, kModuleModelColumnsIncompatible, {});
            emit              dataChanged(compatibility_start, compatibility_end);
        }
        else
        {
            emit ModuleFailedToChangeStatus(mutable_module, is_enabled);
        }
    }

    void ModuleModel::LockModules()
    {
        modules_locked_ = true;
        emit ModulesLocked();
    }

    void ModuleModel::UnlockModules()
    {
        modules_locked_ = false;
        emit ModulesUnlocked();
    }

    bool ModuleModel::AreModulesLocked() const
    {
        return modules_locked_;
    }

    void ModuleModel::ResetModuleToDefault(const DevToolsModule* module) const
    {
        const int module_index = GetModuleIndex(module->GetModuleName());
        if (module_index == -1)
        {
            return;
        }

        DevToolsModule* mutable_module = modules_[module_index];
        mutable_module->ResetToDefaults();
    }

    void ModuleModel::SetModuleData(const DevToolsModule* module, const std::string& data) const
    {
        const int module_index = GetModuleIndex(module->GetModuleName());
        if (module_index == -1)
        {
            return;
        }

        DevToolsModule* mutable_module = modules_[module_index];
        const bool      result         = mutable_module->LoadSerializedData(data);

        Q_ASSERT(result == true);
    }

    bool ModuleModel::IsApiSupportedByEnabledModules(devtrace::Api api)
    {
        for (const DevToolsModule* module : modules_)
        {
            if (module->NeedsDriverConnections() && module->IsCompatibleWithApi(api))
            {
                return true;
            }
        }

        return false;
    }

    bool ModuleModel::IsModuleCompatibleWithEnabledModules(const DevToolsModule* module) const
    {
        return GetIncompatibleEnabledModules(module).empty();
    }

    std::vector<const DevToolsModule*> ModuleModel::GetIncompatibleEnabledModules(const DevToolsModule* module) const
    {
        std::vector<const DevToolsModule*> incompatible_modules{};
        for (const DevToolsModule* candidate : modules_)
        {
            if (!candidate->IsEnabled())
            {
                continue;
            }

            // Both modules need to be compatible with each other in order to ensure that there are no conflicts
            if (!module->IsCompatibleWithModuleNamed(candidate->GetModuleName()) || !candidate->IsCompatibleWithModuleNamed(module->GetModuleName()))
            {
                incompatible_modules.push_back(candidate);
            }
        }

        return incompatible_modules;
    }

    void ModuleModel::SetDisplayApplication(const QString& application_name) const
    {
        for (DevToolsModule* module : modules_)
        {
            module->SetDisplayApplication(application_name);
        }
    }

    QModelIndex ModuleModel::index(const int row, const int column, const QModelIndex& parent) const
    {
        if (!hasIndex(row, column, parent) || parent.isValid())
        {
            return {};
        }

        return createIndex(row, column);
    }

    QModelIndex ModuleModel::parent(const QModelIndex& child) const
    {
        Q_UNUSED(child);
        return {};
    }

    int ModuleModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid())
        {
            return 0;
        }

        return static_cast<int>(modules_.size());
    }

    int ModuleModel::columnCount([[maybe_unused]] const QModelIndex& parent) const
    {
        return kModuleModelColumnsCount;
    }

    QVariant ModuleModel::data(const QModelIndex& index, const int role) const
    {
        if (index.parent().isValid())
        {
            return {};
        }

        Q_ASSERT(index.row() < static_cast<int>(modules_.size()));
        const DevToolsModule* module = modules_[index.row()];

        switch (role)
        {
        case Qt::DisplayRole:
            return DisplayData(module, index.column());
        case Qt::ToolTipRole:
            return TooltipData(module, index.column());
        default:
            return {};
        }
    }

    QVariant ModuleModel::DisplayData(const DevToolsModule* module, const int column) const
    {
        switch (column)
        {
        case kModuleModelColumnsDisplayName:
            return module->GetModuleDisplayName().c_str();
        case kModuleModelColumnsEnabled:
            return module->IsEnabled();
        case kModuleModelColumnsIncompatible:
            return !IsModuleCompatibleWithEnabledModules(module);
        default:
            return {};
        }
    }

    QVariant ModuleModel::TooltipData(const DevToolsModule* module, [[maybe_unused]] const int column) const
    {
        const std::vector<const DevToolsModule*> incompatible_modules = GetIncompatibleEnabledModules(module);
        if (incompatible_modules.empty())
        {
            return {};
        }

        const QString module_display_name = module->GetModuleDisplayName().c_str();
        QStringList   display_names;

        for (const DevToolsModule* incompatible_module : incompatible_modules)
        {
            display_names.push_back(incompatible_module->GetModuleDisplayName().c_str());
        }

        if (display_names.size() == 1)
        {
            return QString("%1 is not compatible with %2").arg(module_display_name, display_names[0]);
        }

        const QString last_module = display_names.last();
        display_names.removeLast();

        return QString("%1 is not compatible with %2 and %3").arg(module_display_name, display_names.join(", "), last_module);
    }

    const DevToolsModule* ModuleModel::GetModuleAtIndex(const QModelIndex& index) const
    {
        if (index.parent().isValid())
        {
            return nullptr;
        }

        Q_ASSERT(index.row() < static_cast<int>(modules_.size()));
        return modules_[index.row()];
    }
}  // namespace rdp
