// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Profiling module config declarations

#include "Profiling.h"

#include <dd_driver_utils_api.h>
#include <dd_uber_trace_api.h>

#include <chunk_writing.h>
#include <client_connection_manager.h>
#include <source_userdata_mapper.h>
#include <trace_source_factory.h>

#include <common/inc/model/file_utils.h>
#include <common/inc/model/mercury_logger.h>
#include <common/inc/model/qt_trace_io.h>
#include <common/inc/model/qt_trace_timer.h>
#include <common/inc/model/trace_file_opener.h>

#include "common/inc/definitions.h"
#include "gui/profiling_prelaunch_settings_helper.h"
#include "gui/profiling_userdata_view.h"
#include "gui/profiling_userdata_view_model.h"
#include "gui/profiling_view.h"

DD_RESULT DDModuleLoad_Profiling(DDApiRegistry* pApiRegistry)
{
    // DevTools modules will delete themselves when the module is destroyed by DevDriver.
    new ProfilingDevToolsModule(pApiRegistry);

    return DD_RESULT_SUCCESS;
}

ProfilingDevToolsModule::ProfilingDevToolsModule(DDApiRegistry* api_registry)
    : DevToolsModule(api_registry, "Profiling", "Profiling")
{
}

DD_RESULT ProfilingDevToolsModule::Initialize(QSettings* tool_settings)
{
    DDGpuProfilingApi* profiling_api = nullptr;
    DD_RESULT          result        = api_registry_->Get(api_registry_->pInstance,
                                          DD_GPU_PROFILING_API_NAME,
                                          DDVersion{DD_GPU_PROFILING_API_VERSION_MAJOR, DD_GPU_PROFILING_API_VERSION_MINOR, DD_GPU_PROFILING_API_VERSION_PATCH},
                                          reinterpret_cast<void**>(&profiling_api));
    if (result != DD_RESULT_SUCCESS)
    {
        return result;
    }

    DDUberTraceApi* uber_trace_api = nullptr;
    result                         = api_registry_->Get(api_registry_->pInstance,
                                DD_UBER_TRACE_API_NAME,
                                DDVersion{DD_UBER_TRACE_API_VERSION_MAJOR, DD_UBER_TRACE_API_VERSION_MINOR, DD_UBER_TRACE_API_VERSION_PATCH},
                                reinterpret_cast<void**>(&uber_trace_api));

    if (result != DD_RESULT_SUCCESS)
    {
        return result;
    }

    auto system_info_cache       = std::make_shared<devtrace::SystemInfoCache>(router_utils_api_);
    auto additional_chunk_writer = std::make_shared<devtrace::AdditionalChunkWriter>(system_info_cache, router_utils_api_);

    auto stream_provider     = std::make_shared<QFileStreamProvider>(kProfileExtension);
    auto file_opener         = std::make_shared<QProcessTraceFileOpener>();
    auto file_utils          = std::make_shared<QtFileUtils>();
    auto timer               = std::make_shared<QTraceTimer>();
    auto logger              = std::make_shared<MercuryLogger>(logging_api_);
    auto spm_counter_handler = std::make_shared<devtrace::GpaSpmCounterHandler>(stream_provider, logger);

    trace_source_ = devtrace::TraceSourceFactory::CreateRgpSource(stream_provider,
                                                                  overlay_manager_,
                                                                  system_info_cache,
                                                                  additional_chunk_writer,
                                                                  spm_counter_handler,
                                                                  device_clocks_manager_,
                                                                  profiling_api,
                                                                  uber_trace_api,
                                                                  ubertrace_factory_,
                                                                  driver_utils_api_,
                                                                  logger);

    base_trace_source_ = trace_source_;

    auto mapper           = std::make_shared<devtrace::RgpUserdataMapper>();
    auto prelaunch_helper = std::make_shared<ProfilingPrelaunchSettingsHelper>(trace_source_);

    userdata_view_model_ = std::make_shared<ProfilingUserdataViewModel>(
        mapper, trace_source_, prelaunch_helper, kRgpProfilesDefaultParentFolder, [&](const std::string& serialized_data) { SerializeData(serialized_data); });

    base_userdata_view_model_ = userdata_view_model_;

    view_model_ =
        std::make_shared<ProfilingViewModel>(stream_provider, trace_source_, file_utils, file_opener, tool_settings, userdata_view_model_, timer, logger);
    base_trace_source_view_model_ = view_model_;

    return connection_manager_v2_->Initialize(trace_source_) ? DD_RESULT_SUCCESS : DD_RESULT_UNKNOWN;
}

bool ProfilingDevToolsModule::Enable()
{
    const std::string setter_name = "Profiling";
    const DD_RESULT   result      = driver_utils_api_->SetFeature(driver_utils_api_->pInstance,
                                                           DD_DRIVER_UTILS_FEATURE_TRACING,
                                                           DD_DRIVER_UTILS_FEATURE_FLAG_ENABLE,
                                                           setter_name.c_str(),
                                                           static_cast<uint32_t>(setter_name.size()));

    return result == DD_RESULT_SUCCESS;
}

bool ProfilingDevToolsModule::Disable()
{
    const std::string setter_name = "Profiling";
    const DD_RESULT   result      = driver_utils_api_->SetFeature(driver_utils_api_->pInstance,
                                                           DD_DRIVER_UTILS_FEATURE_TRACING,
                                                           DD_DRIVER_UTILS_FEATURE_FLAG_IGNORE,
                                                           setter_name.c_str(),
                                                           static_cast<uint32_t>(setter_name.size()));

    return result == DD_RESULT_SUCCESS;
}

QWidget* ProfilingDevToolsModule::CreateView() const
{
    const auto utility_view = new ProfilingUserdataView();

    const auto view = new ProfilingView(utility_view, view_model_, userdata_view_model_);
    view->SetUserdataViewModel(userdata_view_model_);

    utility_view->SetModel(userdata_view_model_);
    return view;
}

bool ProfilingDevToolsModule::IsCompatibleWithApi(devtrace::Api api) const
{
    return api == devtrace::Api::kDirectX12 || api == devtrace::Api::kVulkan || api == devtrace::Api::kOpenCl || api == devtrace::Api::kHip;
}
