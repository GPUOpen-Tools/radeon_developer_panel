// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for concrete implementation of the RGP trace source.

#include "rgp_trace_source_impl.h"

#include <array>
#include <utility>
#include <vector>

#include <ddApi.h>

#include "asic_info.h"
#include "rgp_legacy_trace_client.h"
#include "rgp_pal_client_info.h"
#include "rgp_ubertrace_client.h"
#include "ubertrace_factory.h"

// Linux SPM on Navi2x is only supported on driver version 22.40 or newer
static constexpr uint32_t kMinimumLinuxSpmNavi2DriverMajorVersion = 22;
static constexpr uint32_t kMinimumLinuxSpmNavi2DriverMinorVersion = 40;

static constexpr uint32_t kMinimumShaderInstrumentationUniversalDriverMajorVersion = 24;
static constexpr uint32_t kMinimumShaderInstrumentationUniversalDriverMinorVersion = 10;

static constexpr uint32_t kMinimumUberTraceSupportDriverMajorVersion = 24;
static constexpr uint32_t kMinimumUberTraceSupportDriverMinorVersion = 30;

static constexpr uint32_t kMinimumSPMSupportForRdna3_5APUDriverMajorVersion = 25;
static constexpr uint32_t kMinimumSPMSupportForRdna3_5APUDriverMinorVersion = 10;

static constexpr uint32_t kMinimumSPMSupportForRdna3APUDriverMajorVersion = 26;
static constexpr uint32_t kMinimumSPMSupportForRdna3APUDriverMinorVersion = 10;

// ID for Z2 A devices
static constexpr uint32_t kZ2ANavi2Id = 0x163F;

static constexpr uint32_t kMinimumUberTraceSupportOpenCLHipDriverMajorVersion = 25;
static constexpr uint32_t kMinimumUberTraceSupportOpenCLHipDriverMinorVersion = 30;

static constexpr std::array kSpmDisabledGpuSeries = {devtrace::GpuSeries::kRembrandt, devtrace::GpuSeries::kVanGogh, devtrace::GpuSeries::kPhoenix};
static constexpr std::array<devtrace::GpuSeries, 2> kSpmConditionalDisabledRdna3_5ApuSeries = {devtrace::GpuSeries::kStrix, devtrace::GpuSeries::kKrackan};
static constexpr devtrace::GpuSeries                kSpmConditionalDisabledRdna3ApuSeries   = devtrace::GpuSeries::kPhoenix;

namespace
{
    bool IsSpmCaptureSupported(const system_info_utils::GpuInfo& gpu, const system_info_utils::SystemInfo& system_info)
    {
        if (getenv("RDP_ENABLE_SPM_COUNTERS") != nullptr)
        {
            return true;
        }

        const uint32_t            asic_family     = gpu.asic.id_info.family;
        const uint32_t            asic_e_revision = gpu.asic.id_info.e_rev;
        const uint32_t            device_id       = gpu.asic.id_info.device;
        const devtrace::GpuSeries gpu_series      = devtrace::AsicInfo::GetGpuSeries(0, asic_family, asic_e_revision);

        if (const bool is_explicitly_disabled = std::ranges::find(kSpmDisabledGpuSeries, gpu_series) != kSpmDisabledGpuSeries.end();
            gpu_series < devtrace::GpuSeries::kNavi1 || is_explicitly_disabled)
        {
            return false;
        }

        // SPM collection on Navi2x on Linux requires at least a 22.40 driver.
        if (gpu_series >= devtrace::GpuSeries::kNavi2)
        {
            if (const bool is_linux = system_info.os.desc.find("Linux") != std::string::npos;
                is_linux && devtrace::IsDriverTooOld(system_info.driver, kMinimumLinuxSpmNavi2DriverMajorVersion, kMinimumLinuxSpmNavi2DriverMinorVersion))
            {
                return false;
            }
        }

        const bool is_conditionally_disabled =
            std::ranges::find(kSpmConditionalDisabledRdna3_5ApuSeries, gpu_series) != kSpmConditionalDisabledRdna3_5ApuSeries.end();
        if (const bool is_special_hardware = device_id == kZ2ANavi2Id; is_conditionally_disabled || is_special_hardware)
        {
            // Check driver version support for this GPU series
            if (devtrace::IsDriverTooOld(
                    system_info.driver, kMinimumSPMSupportForRdna3_5APUDriverMajorVersion, kMinimumSPMSupportForRdna3_5APUDriverMinorVersion))
            {
                return false;
            }
        }

        if (gpu_series == kSpmConditionalDisabledRdna3ApuSeries)
        {
            // Check driver version support for this GPU series
            if (devtrace::IsDriverTooOld(system_info.driver, kMinimumSPMSupportForRdna3APUDriverMajorVersion, kMinimumSPMSupportForRdna3APUDriverMinorVersion))
            {
                return false;
            }
        }

        return true;
    }

    /// @brief Checks if a GPU supports exec/pop count tokens.
    /// @param [in] gpu The GPU to check.
    /// @return true if the GPU supports exec/pop tokens (RDNA4 or newer), false otherwise.
    bool IsExecPopTokensSupported(const system_info_utils::GpuInfo& gpu)
    {
        const uint32_t device_id   = gpu.asic.id_info.device;
        const uint32_t asic_family = gpu.asic.id_info.family;
        const uint32_t asic_e_rev  = gpu.asic.id_info.e_rev;

        const devtrace::GpuSeries       gpu_series   = devtrace::AsicInfo::GetGpuSeries(device_id, asic_family, asic_e_rev);
        const devtrace::GpuArchitecture architecture = devtrace::AsicInfo::GetGpuArchitecture(gpu_series);

        // Exec/pop count tokens are only supported on RDNA4 (NAVI4X) and newer architectures
        return architecture >= devtrace::GpuArchitecture::kRdna4;
    }

    /// @brief Checks if any GPU in the system supports exec/pop count tokens.
    /// @param [in] system_info The system info to check.
    /// @return true if any GPU supports exec/pop tokens (RDNA4 or newer), false otherwise.
    bool IsExecPopTokensSupported(const system_info_utils::SystemInfo& system_info)
    {
        for (const auto& gpu : system_info.gpus)
        {
            if (IsExecPopTokensSupported(gpu))
            {
                return true;
            }
        }
        return false;
    }
}  // namespace

namespace devtrace
{

    RgpClientFactory::RgpClientFactory(DDGpuProfilingApi*                            profiling_api,
                                       DDUberTraceApi*                               ubertrace_api,
                                       const std::shared_ptr<UbertraceUserFactory>&  ubertrace_factory,
                                       const std::shared_ptr<AdditionalChunkWriter>& addl_chunk_writer,
                                       DDDriverUtilsApi*                             driver_utils_api,
                                       const std::shared_ptr<RgpSpmCounterHandler>&  counter_handler,
                                       const std::shared_ptr<DeviceClocksManager>&   device_clocks_manager)
        : profiling_api_(profiling_api)
        , ubertrace_api_(ubertrace_api)
        , ubertrace_factory_(ubertrace_factory)
        , addl_chunk_writer_(addl_chunk_writer)
        , driver_utils_api_(driver_utils_api)
        , counter_handler_(counter_handler)
        , device_clocks_manager_(device_clocks_manager)
        , driver_info_()
    {
    }

    RgpClientFactory::~RgpClientFactory() = default;

    std::unique_ptr<RgpClient> RgpClientFactory::CreateClient(const ClientConnection& info, ClientUtils<RgpTraceSourceConfigPrivate>& client_utils)
    {
        auto active_gpu_provider = std::make_unique<ActiveGpuProvider>(
            driver_utils_api_, SystemGpuInfo{.gpus = gpus_, .spm_supported_gpus = spm_supported_gpus_}, info.umd_connection_id);

        // Restrict OpenCL/HIP support to 25.10 base drivers
        const bool restrict_compute =
            IsComputeApi(info.api) &&
            IsDriverTooOld(driver_info_, kMinimumUberTraceSupportOpenCLHipDriverMajorVersion, kMinimumUberTraceSupportOpenCLHipDriverMinorVersion);
        if (client_utils.GetClientConfig().ubertrace_supported && !client_utils.GetClientConfig().config.enable_legacy_capture && !restrict_compute)
        {
            return std::make_unique<RgpUbertraceClient>(info,
                                                        client_utils,
                                                        ubertrace_factory_->GetUser(info, ubertrace_api_),
                                                        addl_chunk_writer_,
                                                        active_gpu_provider,
                                                        counter_handler_,
                                                        device_clocks_manager_);
        }

        return std::make_unique<RgpLegacyClient>(info, client_utils, profiling_api_, counter_handler_, active_gpu_provider, addl_chunk_writer_);
    }

    void RgpClientFactory::SetGpus(const std::vector<system_info_utils::GpuInfo>& gpus)
    {
        gpus_ = gpus;
    }

    void RgpClientFactory::SetSpmSupportedGpus(const std::vector<system_info_utils::GpuInfo>& spm_supported_gpus)
    {
        spm_supported_gpus_ = spm_supported_gpus;
    }

    void RgpClientFactory::SetDriver(const system_info_utils::DriverInfo& driver)
    {
        driver_info_ = driver;
    }

    void RgpClientFactory::SetUbertraceFeatures(const UbertraceFeatures& features) const
    {
        ubertrace_factory_->SetUbertraceFeatures(features);
    }

    RgpTraceSourceImpl::RgpTraceSourceImpl(const std::shared_ptr<ReadWriteStreamProvider>& stream_provider,
                                           const std::shared_ptr<OverlayManager>&          overlay_manager,
                                           const std::shared_ptr<SystemInfoCache>&         system_info_cache,
                                           const std::shared_ptr<AdditionalChunkWriter>&   addl_chunk_writer,
                                           const std::shared_ptr<RgpSpmCounterHandler>&    spm_counter_handler,
                                           const std::shared_ptr<DeviceClocksManager>&     device_clocks_manager,
                                           DDGpuProfilingApi*                              profiling_api,
                                           DDUberTraceApi*                                 ubertrace_api,
                                           const std::shared_ptr<UbertraceUserFactory>&    ubertrace_factory,
                                           DDDriverUtilsApi*                               driver_utils_api,
                                           const std::shared_ptr<Logger>&                  logger)
        : system_info_cache_(system_info_cache)
        , counter_handler_(spm_counter_handler)
        , driver_utils_api_(driver_utils_api)
        , logger_(logger->WithSource("RGP Trace Source"))
        , client_factory_(std::make_shared<RgpClientFactory>(profiling_api,
                                                             ubertrace_api,
                                                             ubertrace_factory,
                                                             addl_chunk_writer,
                                                             driver_utils_api,
                                                             counter_handler_,
                                                             device_clocks_manager))
        , base_trace_source_(client_factory_, stream_provider, overlay_manager, OverlayFeature::kRgp, logger_, this)
        , support_event_{}
    {
        base_trace_source_.SetAllClientsDisabledReason(DisabledReason::kHardwareUnsupported);
        base_trace_source_.SetApiFilter({Api::kDirectX12, Api::kVulkan, Api::kOpenCl, Api::kHip});
    }

    void RgpTraceSourceImpl::PostSupportEvent(const RgpTraceSourceSupportEventArgs& args) const
    {
        if (support_event_.listener != nullptr && (support_event_.callback != nullptr))
        {
            support_event_.callback(support_event_.listener, args);
        }
    }

    void RgpTraceSourceImpl::RegisterSupportEvent(const RgpTraceSourceSupportEvent& event)
    {
        const std::lock_guard lock(support_event_mutex_);
        support_event_ = event;
    }

    void RgpTraceSourceImpl::RegisterStatusEvent(const TraceSourceStatusEvent& event)
    {
        base_trace_source_.RegisterStatusEvent(event);
    }

    void RgpTraceSourceImpl::RegisterTraceCompletionEvent(const TraceCompletionEvent& event)
    {
        base_trace_source_.RegisterTraceCompletionEvent(event);
    }

    void RgpTraceSourceImpl::RegisterTraceCaptureProgressEvent(const TraceCaptureProgressEvent& event)
    {
        base_trace_source_.RegisterTraceCaptureProgressEvent(event);
    }

    void RgpTraceSourceImpl::QueryStatus()
    {
        base_trace_source_.QueryStatus();
    }

    void RgpTraceSourceImpl::RouterConnectionStatusChanged(bool is_connected)
    {
        if (!is_connected)
        {
            base_trace_source_.SetDisabledReason(DisabledReason::kEnabled);
            return;
        }

        const auto e_system_info = system_info_cache_->GetSystemInfo();
        if (!e_system_info.has_value())
        {
            RgpTraceSourceSupportEventArgs args{};
            args.is_instruction_tracing_supported                = false;
            args.is_shader_instrumentation_supported_universally = false;
            args.is_spm_capture_supported                        = false;
            args.is_exec_pop_tokens_supported                    = false;
            PostSupportEvent(args);

            base_trace_source_.SetDisabledReason(DisabledReason::kEncounteredError);
            return;
        }

        const auto& system_info = e_system_info.value();

        base_trace_source_.GetConfig().ubertrace_supported =
            !IsDriverTooOld(system_info.driver, kMinimumUberTraceSupportDriverMajorVersion, kMinimumUberTraceSupportDriverMinorVersion);

        RgpTraceSourceSupportEventArgs support_event_args{};
        support_event_args.is_shader_instrumentation_supported_universally = !IsDriverTooOld(
            system_info.driver, kMinimumShaderInstrumentationUniversalDriverMajorVersion, kMinimumShaderInstrumentationUniversalDriverMinorVersion);
        support_event_args.is_instruction_tracing_supported = true;

        // Check if exec/pop tokens are supported (RDNA4/NAVI4X or newer)
        support_event_args.is_exec_pop_tokens_supported = IsExecPopTokensSupported(system_info);

        std::vector<system_info_utils::GpuInfo> spm_supported_gpus;
        for (const auto& gpu : system_info.gpus)
        {
            if (IsSpmCaptureSupported(gpu, system_info))
            {
                spm_supported_gpus.push_back(gpu);
            }
        }
        base_trace_source_.GetConfig().spm_supported_gpus = spm_supported_gpus;

        support_event_args.is_spm_capture_supported = !spm_supported_gpus.empty();
        PostSupportEvent(support_event_args);

        base_trace_source_.SetDisabledReason(DisabledReason::kEnabled);

        client_factory_->SetSpmSupportedGpus(spm_supported_gpus);
        client_factory_->SetGpus(system_info.gpus);
        client_factory_->SetDriver(system_info.driver);
        client_factory_->SetUbertraceFeatures(UbertraceFeatures(system_info));
    }

    void RgpTraceSourceImpl::OnDriverConnected(const DDConnectionInfo& connection_info)
    {
        base_trace_source_.OnDriverConnected(connection_info);
    }

    void RgpTraceSourceImpl::OnDriverDisconnected(DDConnectionId umd_connection_id)
    {
        base_trace_source_.OnDriverDisconnected(umd_connection_id);
    }

    void RgpTraceSourceImpl::OnDriverStateChanged(DDConnectionId umd_connection_id, DD_DRIVER_STATE state)
    {
        base_trace_source_.OnDriverStateChanged(umd_connection_id, state);
    }

    Result RgpTraceSourceImpl::RequestAbortTrace(DDConnectionId umd_connection_id)
    {
        return base_trace_source_.RequestAbortTrace(umd_connection_id);
    }

    Result RgpTraceSourceImpl::RequestAbortProcessing()
    {
        abort_spm_processing_ = true;
        return Result::kSuccess;
    }

    Result RgpTraceSourceImpl::PrepareForDelayedCapture(DDConnectionId connection_id)
    {
        return base_trace_source_.PrepareForDelayedCapture(connection_id);
    }

    Result RgpTraceSourceImpl::RequestBeginTrace(DDConnectionId connection_id, uint32_t capture_mode)
    {
        return base_trace_source_.RequestBeginTrace(connection_id, capture_mode);
    }

    void RgpTraceSourceImpl::GetSupportedCaptureModes(DDConnectionId connection_id, std::vector<uint32_t>& out_modes)
    {
        const AutoCaptureMode auto_capture_mode = GetConfig().auto_capture_mode;

        std::vector<uint32_t> candidates;
        for (uint32_t mode = 0; mode < static_cast<uint32_t>(RgpCaptureMode::kCount); ++mode)
        {
            const auto rgp_mode = static_cast<RgpCaptureMode>(mode);
            if (auto_capture_mode != kAutoCaptureModeNone)
            {
                if ((rgp_mode == RgpCaptureMode::kFrame || rgp_mode == RgpCaptureMode::kDraw) &&
                    (auto_capture_mode == kAutoCaptureModeTimer || (auto_capture_mode == kAutoCaptureModeDispatchIndices)))
                {
                    continue;
                }

                if ((rgp_mode == RgpCaptureMode::kDispatch || rgp_mode == RgpCaptureMode::kDraw) && (auto_capture_mode == kAutoCaptureModeFrameIndex))
                {
                    continue;
                }
            }

            candidates.push_back(mode);
        }

        base_trace_source_.GetSupportedCaptureModes(connection_id, candidates, out_modes);
    }

    RgpTraceSourceConfig& RgpTraceSourceImpl::GetConfig()
    {
        return base_trace_source_.GetConfig().config;
    }

    void RgpTraceSourceImpl::OnTraceCompletedWithSpm(void* listener, const SpmTrace& spm_trace)
    {
        auto* self = static_cast<RgpTraceSourceImpl*>(listener);

        const std::function<void(float)> report_progress = [&](float progress) {
            self->base_trace_source_.EmitPostProcessEvent("Processing counter data...", progress);
        };

        self->abort_spm_processing_ = false;
        auto counter_result =
            self->counter_handler_->GenerateDerivedCounters(spm_trace.path, spm_trace.counters, spm_trace.is_rdf, report_progress, self->abort_spm_processing_);
        if (counter_result != Result::kSuccess)
        {
            self->logger_->LogError("Failed to process derived counters for {}", 0, 0, spm_trace.path);
        }
        else
        {
            self->logger_->LogInfo("Successfully processed derived counters for {}", 0, 0, spm_trace.path);
        }

        TraceCompletionStatus status = TraceCompletionStatus::kCompleted;
        if (counter_result != Result::kSuccess)
        {
            status = self->abort_spm_processing_ ? TraceCompletionStatus::kAborted : TraceCompletionStatus::kError;
        }

        self->base_trace_source_.TraceCompleted(status, spm_trace.path, spm_trace.umd_connection_id);
    }

    void RgpTraceSourceImpl::Bind([[maybe_unused]] RgpClient* client)
    {
        if (const auto spm_client = dynamic_cast<ISpmTraceClient*>(client); spm_client != nullptr)
        {
            spm_client->RegisterSpmTraceListener({this, OnTraceCompletedWithSpm});
        }
    }

    Result RgpTraceSourceImpl::SetShaderInstrumentationEnabled(bool enabled)
    {
        constexpr DD_DRIVER_UTILS_FEATURE  feature = DD_DRIVER_UTILS_FEATURE_SHADER_INSTRUMENTATION;
        const DD_DRIVER_UTILS_FEATURE_FLAG flag    = enabled ? DD_DRIVER_UTILS_FEATURE_FLAG_ENABLE : DD_DRIVER_UTILS_FEATURE_FLAG_IGNORE;

        const std::string setter_name = "RGPTraceSourceImpl";
        std::lock_guard   lock(shader_inst_mutex_);
        const DD_RESULT   result =
            driver_utils_api_->SetFeature(driver_utils_api_->pInstance, feature, flag, setter_name.c_str(), static_cast<uint32_t>(setter_name.size()));

        return result == DD_RESULT_SUCCESS ? Result::kSuccess : Result::kFailure;
    }

}  // namespace devtrace
