// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Memory Trace module config declarations

#include "MemoryTrace.h"

#include <dd_memory_trace_api.h>

#include <chunk_writing.h>
#include <client_connection_manager.h>
#include <trace_source_factory.h>

#include <common/inc/model/mercury_logger.h>
#include <common/inc/view/base_utility_view.h>
#include "common/inc/definitions.h"

#include "gui/memory_trace_module_definitions.h"
#include "gui/memory_trace_userdata_view_model.h"
#include "gui/memory_trace_view.h"
#include "gui/memory_trace_view_model.h"

DD_RESULT DDModuleLoad_MemoryTrace(DDApiRegistry* api_registry)
{
    // DevTools modules will delete themselves when the module is destroyed by DevDriver.
    new MemoryTraceDevToolsModule(api_registry);

    return DD_RESULT_SUCCESS;
}

MemoryTraceDevToolsModule::MemoryTraceDevToolsModule(DDApiRegistry* api_registry)
    : DevToolsModule(api_registry, "MemoryTrace", "Memory Trace")
{
}

DD_RESULT MemoryTraceDevToolsModule::Initialize(class QSettings* tool_settings)
{
    DDMemoryTraceApi* memory_trace_api = nullptr;
    const DD_RESULT   result =
        api_registry_->Get(api_registry_->pInstance,
                           DD_MEMORY_TRACE_API_NAME,
                           DDVersion{DD_MEMORY_TRACE_API_VERSION_MAJOR, DD_MEMORY_TRACE_API_VERSION_MINOR, DD_MEMORY_TRACE_API_VERSION_PATCH},
                           reinterpret_cast<void**>(&memory_trace_api));

    if (result != DD_RESULT_SUCCESS)
    {
        return result;
    }

    auto mapper         = std::make_shared<devtrace::RmvUserdataMapper>();
    utility_view_model_ = std::make_shared<MemoryTraceUserdataViewModel>(
        mapper, kRmvTracesDefaultParentFolder, [&](const std::string& serialized_data) { SerializeData(serialized_data); });

    base_userdata_view_model_ = utility_view_model_;

    auto       system_info_cache       = std::make_shared<devtrace::SystemInfoCache>(router_utils_api_);
    const auto additional_chunk_writer = std::make_shared<devtrace::AdditionalChunkWriter>(system_info_cache, router_utils_api_);
    auto       stream_provider         = std::make_shared<QFileStreamProvider>(kRmvFileExtension);
    auto       logger                  = std::make_shared<MercuryLogger>(logging_api_);

    trace_source_ =
        devtrace::TraceSourceFactory::CreateRmvSource(stream_provider, overlay_manager_, system_info_cache, additional_chunk_writer, memory_trace_api, logger);
    base_trace_source_ = trace_source_;

    auto file_opener = std::make_shared<QProcessTraceFileOpener>();
    auto file_utils  = std::make_shared<QtFileUtils>();

    memory_trace_model_           = std::make_shared<MemoryTraceViewModel>(stream_provider, trace_source_, file_utils, file_opener, tool_settings, logger);
    base_trace_source_view_model_ = memory_trace_model_;

    return connection_manager_v2_->Initialize(trace_source_) ? DD_RESULT_SUCCESS : DD_RESULT_UNKNOWN;
}

QWidget* MemoryTraceDevToolsModule::CreateView() const
{
    const auto utility_view = new BaseUtilityView();
    utility_view->SetBaseModel(utility_view_model_);

    const auto view = new MemoryTraceView(utility_view, memory_trace_model_);
    view->SetUserdataViewModel(utility_view_model_);

    return view;
}

bool MemoryTraceDevToolsModule::IsCompatibleWithApi(devtrace::Api api) const
{
    return api == devtrace::Api::kDirectX12 || api == devtrace::Api::kVulkan;
}
