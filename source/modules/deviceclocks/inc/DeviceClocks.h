// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief DeviceClocks module config declarations

#ifndef RDP_MODULES_CRASH_ANALYSIS_INC_DEVICE_CLOCKS_H_
#define RDP_MODULES_CRASH_ANALYSIS_INC_DEVICE_CLOCKS_H_

#include <memory>

#include <dd_modules_api.h>

#include <client_connection_manager.h>

#include <common/inc/api/dev_tools_module.h>

/// @brief DevTools Device Clocks gui module interface.
class DeviceClocksDevToolsModule final : public DevToolsModule
{
public:
    /// @brief Default constructor.
    /// @param [in] api_registry The API registry to use for this module
    explicit DeviceClocksDevToolsModule(DDApiRegistry* api_registry);

protected:
    /// @brief Initializes the module.
    /// @param [in] tool_settings The tool settings.
    /// @return DD_RESULT_SUCCESS if the module initialization was successful.
    DD_RESULT Initialize(QSettings* tool_settings) override;

public:
    /// @brief Creates a new device clocks view.
    /// @return A new device clocks view.
    [[nodiscard]] QWidget* CreateView() const override;

    /// @brief Returns the module category.
    /// @return ModuleCategory::kSystem since device clocks operates system-wide.
    [[nodiscard]] ModuleCategory GetModuleCategory() const override;

private:
    std::shared_ptr<class DeviceClocksModel> clocks_model_;  ///< The clocks model.
};

DD_DECLARE_MODULE_LOAD_API(DeviceClocks);

#endif
