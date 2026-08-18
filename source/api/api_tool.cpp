// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for the class to handle DevDriver tool.

#include "api_tool.h"

#include <cstdlib>

#include <ddCommon.h>
#include <dd_clocks_api.h>
#include <dd_connection_api.h>
#include <dd_enhanced_crash_info_api.h>
#include <dd_gpu_detective_api.h>
#include <dd_gpu_profiling_api.h>
#include <dd_memory_trace_api.h>
#include <dd_router_utils_api.h>
#include <dd_tool_api.h>
#include <dd_uber_trace_api.h>

#include <g_RouterUtilsModuleInterface.h>
#include <g_SiphonModuleInterface.h>
#include <g_SystemTraceModuleStatic.h>

std::unique_ptr<ApiTool> ApiTool::CreateTool()
{
    std::unique_ptr<ApiTool> tool = std::unique_ptr<ApiTool>(new ApiTool);

    if (!tool->CreateToolApi())
    {
        return nullptr;
    }

    if (!tool->QueryApis())
    {
        return nullptr;
    }

    return tool;
}

bool ApiTool::CreateToolApi()
{
    if (tool_api_ != nullptr)
    {
        return false;
    }

    const char                desc[]      = "RDP Capture API";
    const DDToolApiCreateInfo create_info = DDToolApiCreateInfo{&desc[0], sizeof(desc) - 1, nullptr, 0, nullptr, 0, nullptr};
    return DDToolApiCreate(&create_info, &tool_api_) == DD_RESULT_SUCCESS;
}

#define GET_API(name, type)                                                                                                            \
    {                                                                                                                                  \
        type* api;                                                                                                                     \
        if (api_registry_->Get(api_registry_->pInstance,                                                                               \
                               DD_##name##_API_NAME,                                                                                   \
                               DDVersion{DD_##name##_API_VERSION_MAJOR, DD_##name##_API_VERSION_MINOR, DD_##name##_API_VERSION_PATCH}, \
                               reinterpret_cast<void**>(&api)) != DD_RESULT_SUCCESS)                                                   \
        {                                                                                                                              \
            return false;                                                                                                              \
        }                                                                                                                              \
                                                                                                                                       \
        container.RegisterValue(api);                                                                                                  \
    }

bool ApiTool::QueryApis()
{
    if (tool_api_ == nullptr)
    {
        return false;
    }

    api_registry_ = tool_api_->GetApiRegistry(tool_api_->pInstance);

    GET_API(ROUTER_UTILS, DDRouterUtilsApi);
    GET_API(DRIVER_UTILS, DDDriverUtilsApi);
    GET_API(CONNECTION, DDConnectionApi);

    GET_API(CLOCKS, DDClocksApi);
    GET_API(ENHANCED_CRASH_INFO, DDEnhancedCrashInfoApi);
    GET_API(GPU_DETECTIVE, DDGpuDetectiveApi);
    GET_API(GPU_PROFILING, DDGpuProfilingApi);
    GET_API(MEMORY_TRACE, DDMemoryTraceApi);
    GET_API(SETTINGS, DDSettingsApi);
    GET_API(UBER_TRACE, DDUberTraceApi);
    // Pipelines omitted since it's for internal builds

    return true;
}

ApiTool::~ApiTool()
{
    Destroy();
}

void ApiTool::Destroy()
{
    ddRouterDestroy(router_);
    router_ = DD_API_INVALID_HANDLE;

    DDToolApiDestroy(&tool_api_);
}

bool ApiTool::CreateRouter()
{
    if (router_ != DD_API_INVALID_HANDLE)
    {
        return false;
    }

    DDRouterCreateInfo router_create_info{};
    router_create_info.pDescription = "RDP Capture API Router";
    router_create_info.alloc        = {ddApiDefaultAlloc, ddApiDefaultFree, nullptr};
    router_create_info.logger       = {};

    if (getenv("RDP_CAPTURE_API_DISABLE_CLIENT_TIMEOUT") != nullptr)
    {
        router_create_info.clientTimeoutCount = static_cast<uint32_t>(-1);
    }

    if (ddRouterCreate(&router_create_info, &router_) != DD_RESULT_SUCCESS)
    {
        return false;
    }

    const std::vector<const DDModuleInterface*> modules = {SystemTraceQueryModule(), RouterUtilsQueryModuleInterface(), SiphonQueryModuleInterface()};
    for (const DDModuleInterface* module : modules)
    {
        DDModuleLoadedInfo module_loaded_info;
        if (ddRouterLoadBuiltinModule(router_, module, &module_loaded_info) != DD_RESULT_SUCCESS)
        {
            return false;
        }
    }

    return true;
}

bool ApiTool::ConnectToRouter(const char* ip, uint16_t port)
{
    return tool_api_->Connect(tool_api_->pInstance, ip, port) == DD_RESULT_SUCCESS;
}

void ApiTool::Disconnect()
{
    tool_api_->Disconnect(tool_api_->pInstance);
}

const dipper::Container& ApiTool::GetApiContainer()
{
    return container;
}

struct DDApiRegistry* ApiTool::GetRegistry()
{
    return api_registry_;
}
