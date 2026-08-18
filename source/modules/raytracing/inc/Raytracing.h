// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Raytracing module config declarations

#ifndef RDP_MODULES_RAYTRACING_INC_RAYTRACING_H_
#define RDP_MODULES_RAYTRACING_INC_RAYTRACING_H_

#include <dd_modules_api.h>

#include <common/inc/api/dev_tools_module.h>

namespace devtrace
{
    class RraTraceSource;
}

/// @brief DevTools Memory Trace gui module interface.
class RaytracingDevToolsModule : public DevToolsModule
{
public:
    /// @brief Default constructor.
    /// @param [in] api_registry The API registry to use for this module
    explicit RaytracingDevToolsModule(DDApiRegistry* api_registry);

protected:
    /// @brief Initializes the module.
    /// @param [in] tool_settings The tool settings.
    /// @return DD_RESULT_SUCCESS if the module initialization was successful.
    DD_RESULT Initialize(class QSettings* tool_settings) override;

    /// @brief Enables the module.
    /// @return true if the module was enabled, false otherwise.
    bool Enable() override;

    /// @brief Disables the module.
    /// @return true if the module was disabled, false otherwise.
    bool Disable() override;

public:
    /// @brief Creates a new raytracing view.
    /// @return A new memory trace view.
    [[nodiscard]] QWidget* CreateView() const override;

    /// @brief Returns whether Raytracing is compatible with the given API.
    /// @param [in] api The API to check module compatibility against.
    /// @return true if Raytracing is compatible with the API, false otherwise.
    [[nodiscard]] bool IsCompatibleWithApi(devtrace::Api api) const override;

private:
    std::shared_ptr<devtrace::RraTraceSource>          trace_source_;         ///< The trace source.
    std::shared_ptr<class RaytracingViewModel>         view_model_;           ///< The raytracing view model.
    std::shared_ptr<class RaytracingUserdataViewModel> userdata_view_model_;  ///< The raytracing userdata view model.
};

DD_DECLARE_MODULE_LOAD_API(Raytracing);

#endif
