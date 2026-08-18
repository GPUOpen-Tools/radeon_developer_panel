// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Memory Trace module config declarations

#ifndef RDP_MODULES_MEMORY_TRACE_INC_MEMORY_TRACE_H_
#define RDP_MODULES_MEMORY_TRACE_INC_MEMORY_TRACE_H_

#include <memory>

#include <dd_modules_api.h>

#include <common/inc/api/dev_tools_module.h>

namespace devtrace
{
    class RmvTraceSource;
}

/// @brief DevTools Memory Trace gui module interface.
class MemoryTraceDevToolsModule : public DevToolsModule
{
public:
    /// @brief Default constructor.
    /// @param [in] api_registry The API registry to use for this module
    explicit MemoryTraceDevToolsModule(DDApiRegistry* api_registry);

protected:
    DD_RESULT Initialize(class QSettings* tool_settings) override;

public:
    [[nodiscard]] QWidget* CreateView() const override;
    [[nodiscard]] bool     IsCompatibleWithApi(devtrace::Api api) const override;

private:
    std::shared_ptr<class MemoryTraceUserdataViewModel> utility_view_model_;  ///< Utility view model for the module.
    std::shared_ptr<devtrace::RmvTraceSource>           trace_source_;        ///< The trace source.
    std::shared_ptr<class MemoryTraceViewModel>         memory_trace_model_;  ///< The memory trace view model.
};

DD_DECLARE_MODULE_LOAD_API(MemoryTrace);

#endif
