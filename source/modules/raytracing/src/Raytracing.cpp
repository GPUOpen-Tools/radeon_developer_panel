// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Raytracing module config declarations

#include "Raytracing.h"

#include <dd_driver_utils_api.h>
#include <dd_uber_trace_api.h>

#include <chunk_writing.h>
#include <client_connection_manager.h>
#include <trace_source_factory.h>

#include <common/inc/definitions.h>
#include <common/inc/model/mercury_logger.h>
#include <common/inc/model/qt_trace_io.h>
#include <common/inc/model/qt_trace_timer.h>

#include "gui/raytracing_module_definitions.h"
#include "gui/raytracing_userdata_view.h"
#include "gui/raytracing_view.h"

// ReSharper disable once CppInconsistentNaming
DD_RESULT DDModuleLoad_Raytracing(DDApiRegistry* pApiRegistry)
{
    // DevTools modules will delete themselves when the module is destroyed by DevDriver.
    new RaytracingDevToolsModule(pApiRegistry);

    return DD_RESULT_SUCCESS;
}

RaytracingDevToolsModule::RaytracingDevToolsModule(DDApiRegistry* api_registry)
    : DevToolsModule(api_registry, "Raytracing", "Raytracing")
{
}

DD_RESULT RaytracingDevToolsModule::Initialize(QSettings* tool_settings)
{
    DDUberTraceApi* uber_trace_api = nullptr;
    const DD_RESULT result         = api_registry_->Get(api_registry_->pInstance,
                                                DD_UBER_TRACE_API_NAME,
                                                DDVersion{DD_UBER_TRACE_API_VERSION_MAJOR, DD_UBER_TRACE_API_VERSION_MINOR, DD_UBER_TRACE_API_VERSION_PATCH},
                                                reinterpret_cast<void**>(&uber_trace_api));

    if (result != DD_RESULT_SUCCESS)
    {
        return result;
    }

    auto       system_info_cache       = std::make_shared<devtrace::SystemInfoCache>(router_utils_api_);
    const auto additional_chunk_writer = std::make_shared<devtrace::AdditionalChunkWriter>(system_info_cache, router_utils_api_);
    auto       stream_provider         = std::make_shared<QFileStreamProvider>(kRraFileExtension);
    auto       file_opener             = std::make_shared<QProcessTraceFileOpener>();
    auto       file_utils              = std::make_shared<QtFileUtils>();
    auto       timer                   = std::make_shared<QTraceTimer>();
    auto       logger                  = std::make_shared<MercuryLogger>(logging_api_);

    trace_source_ = devtrace::TraceSourceFactory::CreateRraSource(
        stream_provider, overlay_manager_, system_info_cache, additional_chunk_writer, uber_trace_api, ubertrace_factory_, logger);

    base_trace_source_ = trace_source_;

    auto mapper           = std::make_shared<devtrace::RraUserdataMapper>();
    auto prelaunch_helper = std::make_shared<TraceSourcePrelaunchSettingsHelper>(trace_source_);

    userdata_view_model_ = std::make_shared<RaytracingUserdataViewModel>(
        mapper, trace_source_, prelaunch_helper, kRraScenesDefaultParentFolder, [&](const std::string& serialized_data) { SerializeData(serialized_data); });

    base_userdata_view_model_ = userdata_view_model_;

    view_model_ =
        std::make_shared<RaytracingViewModel>(stream_provider, trace_source_, file_utils, file_opener, tool_settings, userdata_view_model_, timer, logger);

    base_trace_source_view_model_ = view_model_;

    return connection_manager_v2_->Initialize(trace_source_) ? DD_RESULT_SUCCESS : DD_RESULT_UNKNOWN;
}

bool RaytracingDevToolsModule::Enable()
{
    const std::string setter_name = "Raytracing";
    const DD_RESULT   result      = driver_utils_api_->SetFeature(driver_utils_api_->pInstance,
                                                           DD_DRIVER_UTILS_FEATURE_TRACING,
                                                           DD_DRIVER_UTILS_FEATURE_FLAG_ENABLE,
                                                           setter_name.c_str(),
                                                           static_cast<uint32_t>(setter_name.size()));

    return result == DD_RESULT_SUCCESS;
}

bool RaytracingDevToolsModule::Disable()
{
    const std::string setter_name = "Raytracing";
    const DD_RESULT   result      = driver_utils_api_->SetFeature(driver_utils_api_->pInstance,
                                                           DD_DRIVER_UTILS_FEATURE_TRACING,
                                                           DD_DRIVER_UTILS_FEATURE_FLAG_IGNORE,
                                                           setter_name.c_str(),
                                                           static_cast<uint32_t>(setter_name.size()));

    return result == DD_RESULT_SUCCESS;
}

QWidget* RaytracingDevToolsModule::CreateView() const
{
    auto* utility_view = new RaytracingUserdataView();

    auto* view = new RaytracingView(utility_view, view_model_, userdata_view_model_);
    view->SetUserdataViewModel(userdata_view_model_);

    utility_view->SetModel(userdata_view_model_);

    return view;
}

bool RaytracingDevToolsModule::IsCompatibleWithApi(const devtrace::Api api) const
{
    return api == devtrace::Api::kDirectX12 || api == devtrace::Api::kVulkan;
}
