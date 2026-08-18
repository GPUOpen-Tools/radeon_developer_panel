// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the concrete Rgd trace source.

#include "rgd_trace_source_impl.h"

#include <algorithm>
#include <array>
#include <string>
#include <unordered_set>

#include <ddApi.h>

#include "asic_info.h"
#include "chunk_writing.h"
#include "logging_definitions.h"
#include "rgd_ubertrace_client.h"
#include "system_info_cache.h"

namespace devtrace
{
    static constexpr uint32_t kMinimumHardwareCrashAnalysisMajorVersion = 24;
    static constexpr uint32_t kMinimumHardwareCrashAnalysisMinorVersion = 30;

    // Wave SGPR/VGPR collection during crash analysis requires driver 25.20 or newer. Older drivers
    // accept the capture flags but never emit GPR data.
    static constexpr uint32_t kMinimumGprCaptureMajorVersion = 25;
    static constexpr uint32_t kMinimumGprCaptureMinorVersion = 20;

    static constexpr int kMinimumCrashAnalysisDriverMajorVersion = 23;
    static constexpr int kMinimumCrashAnalysisDriverMinorVersion = 10;

    static constexpr uint32_t kMinimumCrashAnalysisVulkanDriverMajorVersion = 23;
    static constexpr uint32_t kMinimumCrashAnalysisVulkanDriverMinorVersion = 30;

    static constexpr uint32_t kMinimumCrashAnalysisUbertraceDriverMajorVersion = 24;
    static constexpr uint32_t kMinimumCrashAnalysisUbertraceDriverMinorVersion = 20;

    static constexpr uint32_t kUnsupported6750XtRevision = 0x000000C0;
    static constexpr uint32_t kUnsupported6750XtDevice   = 0x000073DF;

    static constexpr uint32_t                  kVanGoghHandheldDeviceId   = 0x163F;
    static constexpr uint32_t                  kVanGoghHandheldRevisionId = 0xAF;
    [[maybe_unused]] static constexpr uint32_t kStrixHandheldDeviceId     = 0x150E;
    [[maybe_unused]] static constexpr uint32_t kStrixHandheldRevisionId1  = 0xC5;
    [[maybe_unused]] static constexpr uint32_t kStrixHandheldRevisionId2  = 0XC7;

    /// Unsupported Device:Revision hardware
    static constexpr std::array<std::pair<uint32_t, uint32_t>, 1> kUnsupportedHandheldDevices = {{{kVanGoghHandheldDeviceId, kVanGoghHandheldRevisionId}}};

    static constexpr uint32_t kStrix1Id    = 0x150E;
    static constexpr uint32_t kStrixHaloId = 0x1586;
    static constexpr uint32_t kKrackan1Id  = 0x1114;
    static constexpr uint32_t kKrackan2Id  = 0x1902;

    static const std::unordered_set kRgdSupportedApis          = {Api::kDirectX12, Api::kVulkan};
    static constexpr std::array     kRgdSupportedArchitectures = {GpuArchitecture::kRdna2, GpuArchitecture::kRdna3, GpuArchitecture::kRdna4};
    static constexpr std::array     kHardwareApuSeries         = {GpuSeries::kRembrandt, GpuSeries::kPhoenix, GpuSeries::kStrix, GpuSeries::kKrackan};
    static constexpr std::array     kHcaAlwaysDisabled         = {GpuSeries::kRembrandt, GpuSeries::kPhoenix};
    static constexpr std::array     kHcaEnabled2520OrNewer     = {kStrixHaloId, kKrackan1Id, kKrackan2Id};

    RgdClientFactory::RgdClientFactory(DDGpuDetectiveApi*                            gpu_detective_api,
                                       DDUberTraceApi*                               ubertrace_api,
                                       const std::shared_ptr<UbertraceUserFactory>&  ubertrace_factory,
                                       DDEnhancedCrashInfoApi*                       enhanced_api,
                                       const std::shared_ptr<AdditionalChunkWriter>& additional_chunk_writer)
        : gpu_detective_api_(gpu_detective_api)
        , ubertrace_api_(ubertrace_api)
        , ubertrace_factory_(ubertrace_factory)
        , enhanced_api_(enhanced_api)
        , additional_chunk_writer_(additional_chunk_writer)
    {
    }

    RgdClientFactory::~RgdClientFactory() = default;

    std::unique_ptr<RgdClient> RgdClientFactory::CreateClient(const ClientConnection& info, ClientUtils<RgdTraceSourceConfig>& client_utils)
    {
        std::unique_ptr<RgdUbertraceClient> ubertrace_client;
        if (enable_ubertrace_)
        {
            auto wrapped_client_utils = std::make_unique<RgdUbertraceClientUtilsWrapper>(client_utils);
            ubertrace_client = std::make_unique<RgdUbertraceClient>(info, std::move(wrapped_client_utils), ubertrace_factory_->GetUser(info, ubertrace_api_));
        }

        return std::make_unique<RgdClient>(info, client_utils, gpu_detective_api_, enhanced_api_, std::move(ubertrace_client), additional_chunk_writer_);
    }

    void RgdClientFactory::SetUbertraceEnabled(const bool enabled)
    {
        enable_ubertrace_ = enabled;
    }

    void RgdClientFactory::SetUbertraceFeatures(const UbertraceFeatures& features) const
    {
        ubertrace_factory_->SetUbertraceFeatures(features);
    }

    //    void RgdTraceSourceImpl::SystemInfoCallback(void* userdata, const char* json_str)
    //    {
    //        RgdTraceSourceImpl* source = reinterpret_cast<RgdTraceSourceImpl*>(userdata);
    //
    //        system_info_utils::SystemInfo system_info;
    //        if (!system_info_utils::SystemInfoReader::Parse(json_str, system_info))
    //        {
    //            source->logger_->LogError("Failed to parse system info, defaulting to unsupported.", 0);
    //
    //            source->base_trace_source_.SystemDisabledReason().set_value(DisabledReason::kEncounteredError);
    //
    //            return;
    //        }
    //
    //        const DisabledReason disabled_reason = GetReasonRgdShouldBeDisabled(system_info);
    //        source->base_trace_source_.SystemDisabledReason().set_value(disabled_reason);
    //
    //        if (disabled_reason != DisabledReason::kEnabled)
    //        {
    //            source->logger_->LogWarning("System info received and parsed, but system does not support RGD.", 0);
    //            return;
    //        }
    //
    //        std::unordered_set<Api> supported_apis = kRgdSupportedApis;
    //        if (IsDriverTooOld(system_info.driver, kMinimumCrashAnalysisVulkanDriverMajorVersion, kMinimumCrashAnalysisVulkanDriverMinorVersion))
    //        {
    //            supported_apis.erase(Api::kVulkan);
    //        }
    //
    //        // Check each GPU reported so that we properly handle mGPU systems
    //        bool has_gpu_supporting_hardware_crash_analysis = false;
    //        bool has_only_apu                               = true;
    //        for (const auto& gpu : system_info.gpus)
    //        {
    //            const uint32_t  device_id       = gpu.asic.id_info.device;
    //            const uint32_t  asic_family     = gpu.asic.id_info.family;
    //            const uint32_t  asic_e_revision = gpu.asic.id_info.e_rev;
    //            const GpuSeries gpu_series      = AsicInfo::GetGpuSeries(device_id, asic_family, asic_e_revision);
    //
    //            if (std::find(kHardwareApuSeries.begin(), kHardwareApuSeries.end(), gpu_series) == kHardwareApuSeries.end())
    //            {
    //                has_only_apu = false;
    //            }
    //
    //            // Disable APU series support for HCA expect for Strix 1 devices.
    //            if (std::find(kHardwareCrashAnalysisDisabledGpuSeries.begin(), kHardwareCrashAnalysisDisabledGpuSeries.end(), gpu_series) !=
    //                    kHardwareCrashAnalysisDisabledGpuSeries.end() &&
    //                device_id != kStrix1Id)
    //            {
    //                continue;
    //            }
    //
    //            has_gpu_supporting_hardware_crash_analysis = true;
    //            break;
    //        }
    //        has_gpu_supporting_hardware_crash_analysis = true;
    //        source->current_hardware_is_apu_.set_value(has_only_apu);
    //
    //        const bool driver_supports_hardware_crash_analysis =
    //            !IsDriverTooOld(system_info.driver, kMinimumHardwareCrashAnalysisMajorVersion, kMinimumHardwareCrashAnalysisMinorVersion);
    //        source->hardware_crash_analysis_supported_.set_value(driver_supports_hardware_crash_analysis && has_gpu_supporting_hardware_crash_analysis);
    //
    //        source->base_trace_source_.SetApiFilter(supported_apis);
    //
    //        const bool enable_ubertrace =
    //            !IsDriverTooOld(system_info.driver, kMinimumCrashAnalysisUbertraceDriverMajorVersion, kMinimumCrashAnalysisUbertraceDriverMinorVersion);
    //
    //        source->factory_->SetUbertraceEnabled(enable_ubertrace);
    //        source->factory_->SetUbertraceFeatures(UbertraceFeatures(system_info));
    //    }

    void RgdTraceSourceImpl::OnTraceCompleted(void* userdata, const TraceCompletionEventArgs& args)
    {
        auto* self = static_cast<RgdTraceSourceImpl*>(userdata);
        if (args.result.status == TraceCompletionStatus::kCompleted)
        {
            const bool generate_text = self->GetConfig().generate_text_summary;
            if (const bool generate_json = self->GetConfig().generate_json_summary; generate_text || generate_json)
            {
                self->QueueSummaryGenerationInternal(args.result.path, generate_text, generate_json, true);
            }
        }
    }

    void RgdTraceSourceImpl::RegisterStatusEvent(const TraceSourceStatusEvent& event)
    {
        base_trace_source_.RegisterStatusEvent(event);
    }

    void RgdTraceSourceImpl::RegisterTraceCompletionEvent(const TraceCompletionEvent& event)
    {
        base_trace_source_.RegisterTraceCompletionEvent(event);
    }

    void RgdTraceSourceImpl::RegisterTraceCaptureProgressEvent(const TraceCaptureProgressEvent& event)
    {
        base_trace_source_.RegisterTraceCaptureProgressEvent(event);
    }

    void RgdTraceSourceImpl::QueryStatus()
    {
        base_trace_source_.QueryStatus();
    }

    void RgdTraceSourceImpl::RegisterSupportEvent(const RgdTraceSourceSupportEvent& event)
    {
        const std::scoped_lock lock(support_event_mutex_);
        support_event_ = event;
    }

    void RgdTraceSourceImpl::RegisterSummaryEvent(const RgpSummaryEvent& event)
    {
        const std::scoped_lock lock(summary_event_mutex_);
        summary_event_ = event;
    }

    void RgdTraceSourceImpl::PostSupportEvent(const RgdTraceSourceSupportEventArgs& args) const
    {
        if (support_event_.listener != nullptr && support_event_.callback != nullptr)
        {
            support_event_.callback(support_event_.listener, args);
        }
    }

    void RgdTraceSourceImpl::PostSummaryEvent(const RgdSummaryEventArgs& args) const
    {
        if (summary_event_.listener != nullptr && summary_event_.callback != nullptr)
        {
            summary_event_.callback(summary_event_.listener, args);
        }
    }

    DisabledReason RgdTraceSourceImpl::GetReasonRgdShouldBeDisabled(const system_info_utils::SystemInfo& system_info)
    {
        if (const std::string os_name = system_info.os.name; os_name.find("Windows") == std::string::npos)
        {
            return DisabledReason::kOsUnsupported;
        }

        if (IsDriverTooOld(system_info.driver, kMinimumCrashAnalysisDriverMajorVersion, kMinimumCrashAnalysisDriverMinorVersion))
        {
            return DisabledReason::kDriverUnsupported;
        }

        // Check each GPU reported so that we properly handle mGPU systems
        for (const auto& gpu : system_info.gpus)
        {
            const uint32_t asic_family     = gpu.asic.id_info.family;
            const uint32_t asic_e_revision = gpu.asic.id_info.e_rev;
            const uint32_t asic_device     = gpu.asic.id_info.device;
            const uint32_t asic_revision   = gpu.asic.id_info.revision;

            const GpuArchitecture architecture       = AsicInfo::GetGpuArchitecture(AsicInfo::GetGpuSeries(asic_device, asic_family, asic_e_revision));
            const bool            gpu_arch_supported = std::ranges::find(kRgdSupportedArchitectures, architecture) != kRgdSupportedArchitectures.end();

            // Disable support for RX 6750 XT
            bool gpu_device_unsupported = asic_revision == kUnsupported6750XtRevision && asic_device == kUnsupported6750XtDevice;

            // Disable support for handheld devices
            for (const auto& [device_id, revision_id] : kUnsupportedHandheldDevices)
            {
                if (asic_device == device_id && asic_revision == revision_id)
                {
                    gpu_device_unsupported = true;
                    break;
                }
            }

            if (const bool disable_hardware_check = getenv("RDP_DISABLE_HARDWARE_COMPATIBILITY_CHECK") != nullptr;
                (!gpu_arch_supported || gpu_device_unsupported) && !disable_hardware_check)
            {
                continue;
            }

            return DisabledReason::kEnabled;
        }

        return DisabledReason::kHardwareUnsupported;
    }

    RgdTraceSourceImpl::RgdTraceSourceImpl(const std::shared_ptr<ReadWriteStreamProvider>& stream_provider,
                                           const std::shared_ptr<OverlayManager>&          overlay_manager,
                                           const std::shared_ptr<RgdSummaryGenerator>&     summary_generator,
                                           const std::shared_ptr<SystemInfoCache>&         system_info_cache,
                                           const std::shared_ptr<AdditionalChunkWriter>&   additional_chunk_writer,
                                           DDGpuDetectiveApi*                              gpu_detective_api,
                                           DDUberTraceApi*                                 ubertrace_api,
                                           const std::shared_ptr<UbertraceUserFactory>&    ubertrace_factory,
                                           DDEnhancedCrashInfoApi*                         enhanced_api,
                                           const std::shared_ptr<Logger>&                  logger)
        : system_info_cache_(system_info_cache)
        , summary_generator_(summary_generator)
        , logger_(logger->WithSource("RGD Trace Source"))
        , factory_(std::make_shared<RgdClientFactory>(gpu_detective_api, ubertrace_api, ubertrace_factory, enhanced_api, additional_chunk_writer))
        , base_trace_source_(factory_, stream_provider, overlay_manager, OverlayFeature::kRgd, logger_)
    {
        base_trace_source_.SetAllClientsDisabledReason(DisabledReason::kHardwareUnsupported);
        base_trace_source_.SetApiFilter({Api::kDirectX12, Api::kVulkan});

        // Register callback to forward no crash detected events
        base_trace_source_.SetNoCrashDetectedCallback([this] { PostNoCrashDetectedEvent(); });

        // Register callback for completed traces to queue summary generation
        base_trace_source_.RegisterTraceCompletionEvent({.listener = this, .callback = OnTraceCompleted});
    }

    void RgdTraceSourceImpl::RouterConnectionStatusChanged(const bool is_connected)
    {
        if (!is_connected)
        {
            base_trace_source_.SetDisabledReason(DisabledReason::kEnabled);
            base_trace_source_.SetApiFilter(kRgdSupportedApis);
            return;
        }

        const auto e_system_info = system_info_cache_->GetSystemInfo();
        if (!e_system_info.has_value())
        {
            // System info unavailable — driver version unknown, so all capability flags default to false.
            constexpr RgdTraceSourceSupportEventArgs args{};
            PostSupportEvent(args);

            base_trace_source_.SetDisabledReason(DisabledReason::kEncounteredError);
            return;
        }

        const auto& system_info = e_system_info.value();

        const DisabledReason disabled_reason = GetReasonRgdShouldBeDisabled(system_info);
        base_trace_source_.SetDisabledReason(disabled_reason);

        if (disabled_reason != DisabledReason::kEnabled)
        {
            logger_->LogWarning("System info received and parsed, but system does not support RGD.", kLoggingInvalidPid, kLoggingInvalidUmdId);

            // RGD is disabled on this system (unsupported OS, GPU, or driver version); capability flags default to false.
            constexpr RgdTraceSourceSupportEventArgs args{};
            PostSupportEvent(args);
            return;
        }

        std::unordered_set<Api> supported_apis = kRgdSupportedApis;
        if (IsDriverTooOld(system_info.driver, kMinimumCrashAnalysisVulkanDriverMajorVersion, kMinimumCrashAnalysisVulkanDriverMinorVersion))
        {
            supported_apis.erase(Api::kVulkan);
        }

        // Check each GPU reported so that we properly handle mGPU systems
        bool has_gpu_supporting_hardware_crash_analysis = false;
        bool has_only_apu                               = true;
        for (const auto& gpu : system_info.gpus)
        {
            const uint32_t  device_id       = gpu.asic.id_info.device;
            const uint32_t  asic_family     = gpu.asic.id_info.family;
            const uint32_t  asic_e_revision = gpu.asic.id_info.e_rev;
            const GpuSeries gpu_series      = AsicInfo::GetGpuSeries(device_id, asic_family, asic_e_revision);

            if (std::ranges::find(kHardwareApuSeries, gpu_series) == kHardwareApuSeries.end())
            {
                has_only_apu = false;
            }

            // Disable APU series support for HCA.
            if (std::ranges::find(kHcaAlwaysDisabled, gpu_series) != kHcaAlwaysDisabled.end())
            {
                continue;
            }

            // Disable Strix1 support if not using 25.10
            if (const bool can_enable_hca_strix1 = !IsDriverTooOld(system_info.driver, 25, 10); device_id == kStrix1Id && !can_enable_hca_strix1)
            {
                continue;
            }

            // Disable Strix Halo or Krackan support if not using 25.20 or newer
            const bool can_enable_strix_halo_krackan = !IsDriverTooOld(system_info.driver, 25, 20);
            if (const bool is_strix_halo_or_krackan = std::ranges::find(kHcaEnabled2520OrNewer, device_id) != kHcaEnabled2520OrNewer.end();
                is_strix_halo_or_krackan && !can_enable_strix_halo_krackan)
            {
                continue;
            }

            has_gpu_supporting_hardware_crash_analysis = true;
            break;
        }

        const bool driver_supports_hardware_crash_analysis =
            !IsDriverTooOld(system_info.driver, kMinimumHardwareCrashAnalysisMajorVersion, kMinimumHardwareCrashAnalysisMinorVersion);

        base_trace_source_.SetApiFilter(supported_apis);

        const bool enable_ubertrace =
            !IsDriverTooOld(system_info.driver, kMinimumCrashAnalysisUbertraceDriverMajorVersion, kMinimumCrashAnalysisUbertraceDriverMinorVersion);

        factory_->SetUbertraceEnabled(enable_ubertrace);
        factory_->SetUbertraceFeatures(UbertraceFeatures(system_info));

        const bool driver_supports_gpr_capture = !IsDriverTooOld(system_info.driver, kMinimumGprCaptureMajorVersion, kMinimumGprCaptureMinorVersion);

        RgdTraceSourceSupportEventArgs support_event_args{};
        support_event_args.is_hardware_crash_analysis_supported = driver_supports_hardware_crash_analysis && has_gpu_supporting_hardware_crash_analysis;
        support_event_args.is_hardware_apu                      = has_only_apu;
        support_event_args.is_gpr_capture_supported             = driver_supports_gpr_capture;

        // Store the APU state
        current_hardware_is_apu_.store(has_only_apu);
        hardware_crash_analysis_supported_.store(has_gpu_supporting_hardware_crash_analysis);

        base_trace_source_.GetConfig().hardware_crash_analysis_supported = support_event_args.is_hardware_crash_analysis_supported;

        PostSupportEvent(support_event_args);
    }

    void RgdTraceSourceImpl::OnDriverConnected(const DDConnectionInfo& connection_info)
    {
        base_trace_source_.OnDriverConnected(connection_info);
    }

    void RgdTraceSourceImpl::OnDriverDisconnected(const DDConnectionId umd_connection_id)
    {
        base_trace_source_.OnDriverDisconnected(umd_connection_id);
    }

    void RgdTraceSourceImpl::OnDriverStateChanged(const DDConnectionId umd_connection_id, const DD_DRIVER_STATE state)
    {
        base_trace_source_.OnDriverStateChanged(umd_connection_id, state);
    }

    Result RgdTraceSourceImpl::RequestAbortTrace(DDConnectionId umd_connection_id)
    {
        return base_trace_source_.RequestAbortTrace(umd_connection_id);
    }

    Result RgdTraceSourceImpl::RequestAbortProcessing()
    {
        should_abort_summary_ = true;
        return Result::kSuccess;
    }

    RgdTraceSourceConfig& RgdTraceSourceImpl::GetConfig()
    {
        return base_trace_source_.GetConfig();
    }

    Result RgdTraceSourceImpl::AddMarker(const DDConnectionId connection_id, const std::string& marker)
    {
        return base_trace_source_.AddMarker(connection_id, marker);
    }

    Result RgdTraceSourceImpl::RequestDump(const DDConnectionId connection_id)
    {
        return base_trace_source_.RequestDump(connection_id);
    }

    Result RgdTraceSourceImpl::QueueSummaryGeneration(const std::string& path, const bool generate_text, const bool generate_json)
    {
        return QueueSummaryGenerationInternal(path, generate_text, generate_json, false);
    }

    void RgdTraceSourceImpl::RegisterNoCrashDetectedEvent(const RgdNoCrashDetectedEvent& event)
    {
        const std::scoped_lock lock(no_crash_detected_event_mutex_);
        no_crash_detected_event_ = event;
    }

    void RgdTraceSourceImpl::PostNoCrashDetectedEvent() const
    {
        const std::scoped_lock lock(no_crash_detected_event_mutex_);
        if (no_crash_detected_event_.listener != nullptr && no_crash_detected_event_.callback)
        {
            no_crash_detected_event_.callback(no_crash_detected_event_.listener);
        }
    }

    bool RgdTraceSourceImpl::IsCurrentHardwareApu() const
    {
        return current_hardware_is_apu_.load();
    }

    bool RgdTraceSourceImpl::IsHardwareCrashAnalysisSupported() const
    {
        return hardware_crash_analysis_supported_.load();
    }

    /* observable<SummaryResult> RgdTraceSourceImpl::GetSummaryResults()
    {
        return summary_result_subject_.get_observable();
    }*/

    Result RgdTraceSourceImpl::QueueSummaryGenerationInternal(const std::string& path, bool generate_text, bool generate_json, bool automatic)
    {
        const RgdSummaryOptions options = base_trace_source_.GetConfig().GetSummaryOptions();

        base_trace_source_.PerformProcessing(true, [this, path, generate_text, generate_json, automatic, options]([[maybe_unused]] auto report_progress) {
            std::string error;
            std::string text_path;
            std::string json_path;

            should_abort_summary_ = false;

            [[maybe_unused]] const Result result =
                summary_generator_->GenerateSummaries(path, generate_text, generate_json, error, text_path, json_path, should_abort_summary_, options);

            if (!text_path.empty())
            {
                PostSummaryEvent({.result = SummaryResult{result, text_path, automatic, error}});
            }

            if (!json_path.empty())
            {
                PostSummaryEvent({.result = SummaryResult{result, json_path, automatic, error}});
            }
        });

        return Result::kSuccess;
    }

}  // namespace devtrace
