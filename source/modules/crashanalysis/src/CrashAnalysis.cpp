// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Crash analysis module config declarations

#include "CrashAnalysis.h"

#include <dd_driver_utils_api.h>
#include <dd_enhanced_crash_info_api.h>
#include <dd_gpu_detective_api.h>
#include <dd_uber_trace_api.h>

#include <chunk_writing.h>
#include <client_connection_manager.h>
#include <system_info_cache.h>
#include <trace_source_factory.h>

#include <common/inc/definitions.h>
#include <common/inc/model/mercury_logger.h>

#include "gui/crash_analysis_module_definitions.h"
#include "gui/crash_analysis_userdata_view.h"
#include "gui/crash_analysis_view.h"
#include "summary/qt_rgd_summary_generator.h"

// ReSharper disable once CppInconsistentNaming
// ReSharper disable once CppParameterNamesMismatch
DD_RESULT DDModuleLoad_CrashAnalysis(DDApiRegistry* api_registry)
{
    // DevTools modules will delete themselves when the module is destroyed by DevDriver.
    new CrashAnalysisDevToolsModule(api_registry);

    return DD_RESULT_SUCCESS;
}

CrashAnalysisDevToolsModule::CrashAnalysisDevToolsModule(DDApiRegistry* api_registry)
    : DevToolsModule(api_registry, "CrashAnalysis", "Crash Analysis")
{
}

DD_RESULT CrashAnalysisDevToolsModule::Initialize(QSettings* tool_settings)
{
    DDGpuDetectiveApi*      gpu_detective_api = nullptr;
    DDEnhancedCrashInfoApi* enhanced_api      = nullptr;

    DD_RESULT result = api_registry_->Get(api_registry_->pInstance,
                                          DD_GPU_DETECTIVE_API_NAME,
                                          DDVersion{DD_GPU_DETECTIVE_API_VERSION_MAJOR, DD_GPU_DETECTIVE_API_VERSION_MINOR, DD_GPU_DETECTIVE_API_VERSION_PATCH},
                                          reinterpret_cast<void**>(&gpu_detective_api));

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

    result = api_registry_->Get(
        api_registry_->pInstance,
        DD_ENHANCED_CRASH_INFO_API_NAME,
        DDVersion{DD_ENHANCED_CRASH_INFO_API_VERSION_MAJOR, DD_ENHANCED_CRASH_INFO_API_VERSION_MINOR, DD_ENHANCED_CRASH_INFO_API_VERSION_PATCH},
        reinterpret_cast<void**>(&enhanced_api));

    if (result != DD_RESULT_SUCCESS)
    {
        return result;
    }

    auto        stream_provider         = std::make_shared<QFileStreamProvider>(kRgdFileExtension);
    auto        file_opener             = std::make_shared<QProcessTraceFileOpener>();
    auto        file_utils              = std::make_shared<QtFileUtils>();
    const auto  summary_generator       = std::make_shared<QtRgdSummaryGenerator>(tool_settings);
    static auto system_info_cache       = std::make_shared<devtrace::SystemInfoCache>(router_utils_api_);
    const auto  additional_chunk_writer = std::make_shared<devtrace::AdditionalChunkWriter>(system_info_cache, router_utils_api_);
    auto        logger                  = std::make_shared<MercuryLogger>(logging_api_);

    trace_source_ = devtrace::TraceSourceFactory::CreateRgdSource(stream_provider,
                                                                  overlay_manager_,
                                                                  summary_generator,
                                                                  system_info_cache,
                                                                  additional_chunk_writer,
                                                                  gpu_detective_api,
                                                                  uber_trace_api,
                                                                  ubertrace_factory_,
                                                                  enhanced_api,
                                                                  logger);

    base_trace_source_ = trace_source_;

    auto mapper          = std::make_shared<devtrace::RgdUserdataMapper>();
    userdata_view_model_ = std::make_shared<CrashAnalysisUserdataViewModel>(
        mapper, trace_source_, nullptr, kRgdTracesDefaultParentFolder, [&](const std::string& serialized_data) { SerializeData(serialized_data); });

    base_userdata_view_model_ = userdata_view_model_;

    crash_analysis_model_ =
        std::make_shared<CrashAnalysisViewModel>(stream_provider, trace_source_, file_utils, file_opener, userdata_view_model_, tool_settings, logger);
    base_trace_source_view_model_ = crash_analysis_model_;

    return connection_manager_v2_->Initialize(trace_source_) ? DD_RESULT_SUCCESS : DD_RESULT_UNKNOWN;
}

bool CrashAnalysisDevToolsModule::Enable()
{
    const std::string setter_name = "CrashAnalysis";
    const DD_RESULT   result      = driver_utils_api_->SetFeature(driver_utils_api_->pInstance,
                                                           DD_DRIVER_UTILS_FEATURE_CRASH_ANALYSIS,
                                                           DD_DRIVER_UTILS_FEATURE_FLAG_ENABLE,
                                                           setter_name.c_str(),
                                                           static_cast<uint32_t>(setter_name.size()));

    return result == DD_RESULT_SUCCESS;
}

bool CrashAnalysisDevToolsModule::Disable()
{
    const std::string setter_name = "CrashAnalysis";
    const DD_RESULT   result      = driver_utils_api_->SetFeature(driver_utils_api_->pInstance,
                                                           DD_DRIVER_UTILS_FEATURE_CRASH_ANALYSIS,
                                                           DD_DRIVER_UTILS_FEATURE_FLAG_IGNORE,
                                                           setter_name.c_str(),
                                                           static_cast<uint32_t>(setter_name.size()));

    return result == DD_RESULT_SUCCESS;
}

QWidget* CrashAnalysisDevToolsModule::CreateView() const
{
    const auto userdata_view = new CrashAnalysisUserdataView();
    userdata_view->SetModel(userdata_view_model_);

    const auto view = new CrashAnalysisView(userdata_view, crash_analysis_model_);
    view->SetUserdataViewModel(userdata_view_model_);

    return view;
}

bool CrashAnalysisDevToolsModule::IsCompatibleWithApi(const devtrace::Api api) const
{
    return api == devtrace::Api::kDirectX12 || api == devtrace::Api::kVulkan;
}

bool CrashAnalysisDevToolsModule::IsCompatibleWithModuleNamed(const std::string& module_name) const
{
    return module_name == "DeviceClocks";
}
