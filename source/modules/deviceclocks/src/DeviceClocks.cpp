// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief DeviceClocks module config declarations

#include "DeviceClocks.h"

#include <QtGlobal>

#include <dd_clocks_api.h>

#include <system_info_cache.h>

#include "gui/device_clocks_model.h"
#include "gui/device_clocks_view.h"

// ReSharper disable once CppInconsistentNaming
// ReSharper disable once CppParameterNamesMismatch
DD_RESULT DDModuleLoad_DeviceClocks(DDApiRegistry* api_registry)
{
    // DevTools modules will delete themselves when the module is destroyed by DevDriver.
    new DeviceClocksDevToolsModule(api_registry);

    return DD_RESULT_SUCCESS;
}

DeviceClocksDevToolsModule::DeviceClocksDevToolsModule(DDApiRegistry* api_registry)
    : DevToolsModule(api_registry, "DeviceClocks", "Device Clocks")
{
}

DD_RESULT DeviceClocksDevToolsModule::Initialize(QSettings* tool_settings)
{
    Q_UNUSED(tool_settings);

    DDClocksApi*    clocks_api = nullptr;
    const DD_RESULT result     = api_registry_->Get(api_registry_->pInstance,
                                                DD_CLOCKS_API_NAME,
                                                DDVersion{DD_CLOCKS_API_VERSION_MAJOR, DD_CLOCKS_API_VERSION_MINOR, DD_CLOCKS_API_VERSION_PATCH},
                                                reinterpret_cast<void**>(&clocks_api));

    if (result != DD_RESULT_SUCCESS)
    {
        return result;
    }

    auto system_info_cache = std::make_shared<devtrace::SystemInfoCache>(router_utils_api_);
    clocks_model_          = std::make_shared<DeviceClocksModel>(device_clocks_manager_);

    return DD_RESULT_SUCCESS;
}

QWidget* DeviceClocksDevToolsModule::CreateView() const
{
    const auto view = new DeviceClocksView();
    view->SetModel(clocks_model_);

    return view;
}

ModuleCategory DeviceClocksDevToolsModule::GetModuleCategory() const
{
    return ModuleCategory::kSystem;
}
