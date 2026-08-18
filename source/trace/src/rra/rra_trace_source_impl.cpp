// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for concrete implementation of the RRA trace source.

#include "rra_trace_source_impl.h"

#include <array>

#include "../base_trace_source/ubertrace_params.h"
#include "asic_info.h"

#include "ubertrace_factory.h"

namespace devtrace
{
    static constexpr int kMinimumRaytracingDriverMajorVersion    = 22;
    static constexpr int kMinimumRaytracingDriverMinorVersion    = 20;
    static constexpr int kMinimumRayHistoryDriverMajorVersion    = 23;
    static constexpr int kMinimumRayHistoryDriverMinorVersion    = 20;
    static constexpr int kMinimumMarkerCaptureDriverMajorVersion = 26;
    static constexpr int kMinimumMarkerCaptureDriverMinorVersion = 20;

    static constexpr std::array kSupportedGpuSeries = {GpuSeries::kNavi2,
                                                       GpuSeries::kNavi3,
                                                       GpuSeries::kRembrandt,
                                                       GpuSeries::kVanGogh,
                                                       GpuSeries::kPhoenix,
                                                       GpuSeries::kStrix,
                                                       GpuSeries::kKrackan,
                                                       GpuSeries::kNavi4};

    RraClient::RraClient(const ClientConnection&                       conn_info,
                         ClientUtils<RraTraceSourceConfigPrivate>&     client_utils,
                         std::unique_ptr<UbertraceUser>                user,
                         const std::shared_ptr<AdditionalChunkWriter>& additional_chunk_writer)
        : UbertraceClient(conn_info, client_utils, std::move(user), additional_chunk_writer, true)
    {
    }

    RraClient::~RraClient() = default;

    void RraClient::GetPreliminarySources(std::vector<UberTraceSource>& sources)
    {
        UberTraceSource accel_src{};
        accel_src.name   = "AccelStruct";
        accel_src.config = std::make_shared<EnableDisableUberTraceSourceConfig>(true);

        UberTraceSource history_src{};
        history_src.name   = "RayHistory";
        history_src.config = std::make_shared<UberTraceRayHistorySourceConfig>(true, 0);

        sources = {accel_src, history_src};
    }

    Result RraClient::GenerateCaptureConfig(const RraTraceSourceConfigPrivate& config, UbertraceCaptureConfig& capture_config)
    {
        if (!SupportsCaptureMode(capture_config.capture_mode))
        {
            return Result::kFailure;
        }

        UberTraceSource asic_info_src{};
        asic_info_src.name   = "asicinfo";
        asic_info_src.config = std::make_shared<UbertraceSourceConfig>();

        UberTraceSource api_info_src{};
        api_info_src.name   = "apiinfo";
        api_info_src.config = std::make_shared<UbertraceSourceConfig>();

        UberTraceSource accel_src{};
        accel_src.name   = "AccelStruct";
        accel_src.config = std::make_shared<EnableDisableUberTraceSourceConfig>(true);

        UberTraceSource history_src{};
        history_src.name = "RayHistory";

        const bool enable_ray_history = config.config.enable_ray_history && config.ray_history_is_supported;
        if (enable_ray_history && config.config.ray_history_buffer_size == 0)
        {
            GetLogger()->LogError("Failed to enable ray history because buffer size was 0 [{} {}]",
                                  GetConnInfo().client_pid,
                                  GetConnInfo().umd_connection_id,
                                  GetConnInfo().umd_connection_id,
                                  GetHumanReadableName(GetConnInfo().api));

            return Result::kFailure;
        }

        history_src.config = std::make_shared<UberTraceRayHistorySourceConfig>(enable_ray_history, config.config.ray_history_buffer_size);

        UberTraceSource stringtable_src{};
        stringtable_src.name   = "stringtable";
        stringtable_src.config = std::make_shared<UbertraceSourceConfig>();

        UberTraceSource user_marker_src{};
        user_marker_src.name   = "usermarkerhist";
        user_marker_src.config = std::make_shared<UbertraceSourceConfig>();

        // Use marker controller if enabled and supported, otherwise fall back to frame controller
        if (config.config.enable_marker_capture && config.config.is_marker_capture_supported)
        {
            UbertraceController marker_controller{};
            marker_controller.name = "marker";
            marker_controller.config =
                std::make_shared<UbertraceMarkerControllerConfig>(true, config.config.marker_begin_string, config.config.marker_end_string);

            capture_config.ubertrace_config.controllers = std::make_unique<UbertraceControllerCollection>(GetFeatures(), marker_controller);
        }
        else
        {
            const UbertraceController frame_controller  = GetFeatures().GetFrameController(true, 0, 1, "relative", 0);
            capture_config.ubertrace_config.controllers = std::make_unique<UbertraceControllerCollection>(GetFeatures(), frame_controller);
        }

        capture_config.ubertrace_config.sources = {asic_info_src, api_info_src, accel_src, history_src, stringtable_src, user_marker_src};

        return Result::kSuccess;
    }

    bool RraClient::SupportsCaptureMode(const uint32_t mode) const
    {
        return mode == 0;
    }

    RraClientFactory::RraClientFactory(DDUberTraceApi*                               api,
                                       const std::shared_ptr<UbertraceUserFactory>&  ubertrace_factory,
                                       const std::shared_ptr<AdditionalChunkWriter>& additional_chunk_writer)
        : api_(api)
        , ubertrace_factory_(ubertrace_factory)
        , additional_chunk_writer_(additional_chunk_writer)
    {
    }

    RraClientFactory::~RraClientFactory() = default;

    std::unique_ptr<RraClient> RraClientFactory::CreateClient(const ClientConnection& info, UtilsType& client_utils)
    {
        return std::make_unique<RraClient>(info, client_utils, ubertrace_factory_->GetUser(info, api_), additional_chunk_writer_);
    }

    void RraClientFactory::SetFeatures(const UbertraceFeatures& features) const
    {
        ubertrace_factory_->SetUbertraceFeatures(features);
    }

    DisabledReason RraTraceSourceImpl::GetReasonRraShouldBeDisabled(const system_info_utils::SystemInfo& system_info)
    {
        if (IsDriverTooOld(system_info.driver, kMinimumRaytracingDriverMajorVersion, kMinimumRaytracingDriverMinorVersion))
        {
            return DisabledReason::kDriverUnsupported;
        }

        if (getenv("RDP_DISABLE_HARDWARE_COMPATIBILITY_CHECK") != nullptr)
        {
            return DisabledReason::kEnabled;
        }

        // Check each GPU reported so that we properly handle mGPU systems
        for (const auto& gpu : system_info.gpus)
        {
            const uint32_t device_id       = gpu.asic.id_info.device;
            const uint32_t asic_family     = gpu.asic.id_info.family;
            const uint32_t asic_e_revision = gpu.asic.id_info.e_rev;

            if (const GpuSeries gpu_series = AsicInfo::GetGpuSeries(device_id, asic_family, asic_e_revision);
                std::ranges::find(kSupportedGpuSeries, gpu_series) == kSupportedGpuSeries.end())
            {
                continue;
            }

            return DisabledReason::kEnabled;
        }

        return DisabledReason::kHardwareUnsupported;
    }

    bool RraTraceSourceImpl::IsRayHistorySupported(const system_info_utils::SystemInfo& system_info)
    {
        return !IsDriverTooOld(system_info.driver, kMinimumRayHistoryDriverMajorVersion, kMinimumRayHistoryDriverMinorVersion);
    }

    bool RraTraceSourceImpl::IsMarkerCaptureSupported(const system_info_utils::SystemInfo& system_info)
    {
        if (!IsDriverTooOld(system_info.driver, kMinimumMarkerCaptureDriverMajorVersion, kMinimumMarkerCaptureDriverMinorVersion))
        {
            return true;
        }

        // Also allow marker capture on the specific pre-release driver 26.10.07.02.
        static constexpr const char* kMarkerCapturePreReleaseDriver = "26.10.07.02";
        return system_info.driver.software_version == kMarkerCapturePreReleaseDriver;
    }

    RraTraceSourceImpl::RraTraceSourceImpl(const std::shared_ptr<ReadWriteStreamProvider>& stream_provider,
                                           const std::shared_ptr<OverlayManager>&          overlay_manager,
                                           const std::shared_ptr<SystemInfoCache>&         system_info_cache,
                                           const std::shared_ptr<AdditionalChunkWriter>&   additional_chunk_writer,
                                           DDUberTraceApi*                                 uber_trace_api,
                                           const std::shared_ptr<UbertraceUserFactory>&    ubertrace_factory,
                                           const std::shared_ptr<Logger>&                  logger)
        : system_info_cache_(system_info_cache)
        , logger_(logger->WithSource("RRA Trace Source"))
        , client_factory_(std::make_shared<RraClientFactory>(uber_trace_api, ubertrace_factory, additional_chunk_writer))
        , base_trace_source_(client_factory_, stream_provider, overlay_manager, OverlayFeature::kRra, logger_)
    {
        base_trace_source_.SetAllClientsDisabledReason(DisabledReason::kHardwareUnsupported);
        base_trace_source_.SetApiFilter({Api::kDirectX12, Api::kVulkan});
    }

    void RraTraceSourceImpl::RouterConnectionStatusChanged(const bool is_connected)
    {
        RraTraceSourceSupportEventArgs args{};
        args.is_ray_history_supported = true;

        if (!is_connected)
        {
            // No connection to router still should allow user to view RRA UI, so set enabled.
            base_trace_source_.SetDisabledReason(DisabledReason::kEnabled);
            PostSupportEvent(args);
            return;
        }

        const auto e_system_info = system_info_cache_->GetSystemInfo();
        if (!e_system_info.has_value())
        {
            logger_->LogError("Failed to query system info.", 0, 0);

            PostSupportEvent(args);

            base_trace_source_.SetDisabledReason(DisabledReason::kEncounteredError);
            return;
        }

        const auto& system_info                                           = e_system_info.value();
        base_trace_source_.GetConfig().ray_history_is_supported           = IsRayHistorySupported(system_info);
        args.is_ray_history_supported                                     = base_trace_source_.GetConfig().ray_history_is_supported;
        base_trace_source_.GetConfig().config.is_marker_capture_supported = IsMarkerCaptureSupported(system_info);
        args.is_marker_capture_supported                                  = base_trace_source_.GetConfig().config.is_marker_capture_supported;
        PostSupportEvent(args);

        base_trace_source_.SetDisabledReason(DisabledReason::kEnabled);
    }

    void RraTraceSourceImpl::PostSupportEvent(const RraTraceSourceSupportEventArgs& args) const
    {
        if (support_event_.listener != nullptr && support_event_.callback != nullptr)
        {
            support_event_.callback(support_event_.listener, args);
        }
    }

    void RraTraceSourceImpl::RegisterSupportEvent(const RraTraceSourceSupportEvent& event)
    {
        const std::lock_guard lock(support_event_mutex_);
        support_event_ = event;
    }

    void RraTraceSourceImpl::RegisterStatusEvent(const TraceSourceStatusEvent& event)
    {
        base_trace_source_.RegisterStatusEvent(event);
    }

    void RraTraceSourceImpl::RegisterTraceCompletionEvent(const TraceCompletionEvent& event)
    {
        base_trace_source_.RegisterTraceCompletionEvent(event);
    }

    void RraTraceSourceImpl::RegisterTraceCaptureProgressEvent(const TraceCaptureProgressEvent& event)
    {
        base_trace_source_.RegisterTraceCaptureProgressEvent(event);
    }

    void RraTraceSourceImpl::QueryStatus()
    {
        base_trace_source_.QueryStatus();
    }

    void RraTraceSourceImpl::OnDriverConnected(const DDConnectionInfo& connection_info)
    {
        base_trace_source_.OnDriverConnected(connection_info);
    }

    void RraTraceSourceImpl::OnDriverDisconnected(const DDConnectionId umd_connection_id)
    {
        base_trace_source_.OnDriverDisconnected(umd_connection_id);
    }

    void RraTraceSourceImpl::OnDriverStateChanged(const DDConnectionId umd_connection_id, const DD_DRIVER_STATE state)
    {
        base_trace_source_.OnDriverStateChanged(umd_connection_id, state);
    }

    Result RraTraceSourceImpl::RequestAbortTrace(DDConnectionId umd_connection_id)
    {
        return base_trace_source_.RequestAbortTrace(umd_connection_id);
    }

    Result RraTraceSourceImpl::RequestAbortProcessing()
    {
        return Result::kSuccess;
    }

    Result RraTraceSourceImpl::PrepareForDelayedCapture(const DDConnectionId connection_id)
    {
        return base_trace_source_.PrepareForDelayedCapture(connection_id);
    }

    Result RraTraceSourceImpl::RequestBeginTrace(const DDConnectionId connection_id, const uint32_t capture_mode)
    {
        return base_trace_source_.RequestBeginTrace(connection_id, capture_mode);
    }

    void RraTraceSourceImpl::GetSupportedCaptureModes(const DDConnectionId connection_id, std::vector<uint32_t>& out_modes)
    {
        base_trace_source_.GetSupportedCaptureModes(connection_id, {0}, out_modes);
    }

    RraTraceSourceConfig& RraTraceSourceImpl::GetConfig()
    {
        return base_trace_source_.GetConfig().config;
    }

}  // namespace devtrace
