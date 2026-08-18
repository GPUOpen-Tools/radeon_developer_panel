// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for trace source adapter.

#include "trace_source_adapter.h"

#include <algorithm>
#include <optional>

#include <trace_source.h>

#include "api_allocator.h"
#include "api_data.h"
#include "api_trace_io.h"

namespace
{
    RdpCaptureApiConnectionId ToCaptureConnectionId(uint16_t conn_id)
    {
        static_assert(sizeof(void*) == sizeof(uint64_t), "Not supported on 32 bit systems");
        return reinterpret_cast<RdpCaptureApiConnectionId*>(conn_id);
    }
}  // namespace

void TraceSourceAdapter::OnTraceCompletionEvent(void* object, const devtrace::TraceCompletionEventArgs& args)
{
    const auto&                     result = args.result;
    auto*                           self   = static_cast<TraceSourceAdapter*>(object);
    RdpFeatureTraceFinishedCallback callback{};
    {
        std::lock_guard lock(self->callback_mutex_);
        callback = self->callback_;
    }

    if (callback.trace_finished == nullptr)
    {
        return;
    }

    const RdpCaptureResult    api_result = GetCaptureResultForTraceCompletionStatus(result.status);
    RdpCaptureApiConnectionId conn_id    = ToCaptureConnectionId(result.umd_connection_id);

    if (api_result != kRdpCaptureResultSuccess)
    {
        if (callback.trace_finished != nullptr)
        {
            callback.trace_finished(callback.user_data, self->feature_, conn_id, api_result, 0, nullptr);
        }

        return;
    }

    uint64_t size = 0;
    uint8_t* data = nullptr;
    if (!self->stream_provider_->ConsumeStream(result.path, size, data))
    {
        // This amounts to a programming error. We should be able to consume the stream since it should be closed.
        DEV_TRACE_ASSERT(false);
        return;
    }

    if (callback.trace_finished != nullptr)
    {
        callback.trace_finished(callback.user_data, self->feature_, conn_id, api_result, size, data);
        ApiFree(data);
    }
}

void TraceSourceAdapter::OnTraceStatusEvent(void* object, const devtrace::TraceSourceStatusEventArgs& args)
{
    auto* self = static_cast<TraceSourceAdapter*>(object);

    const RdpCaptureDetailedStage new_detailed_stage = GetDetailedCaptureStage(args.new_status);
    const RdpCaptureDetailedStage old_detailed_stage = self->current_detailed_stage_;

    self->current_stage_          = GetCaptureStage(args.new_status);
    self->current_detailed_stage_ = new_detailed_stage;
    self->connection_map_         = args.new_status.current_connections;
    self->current_status_         = args.new_status;

    if (new_detailed_stage != old_detailed_stage)
    {
        RdpCaptureFeatureStatusCallback status_cb{};
        {
            std::lock_guard lock(self->callback_mutex_);
            status_cb = self->status_callback_;
        }

        if (status_cb.status_changed != nullptr)
        {
            status_cb.status_changed(status_cb.user_data, self->feature_, new_detailed_stage, old_detailed_stage);
        }
    }
}

void TraceSourceAdapter::OnTraceCaptureProgressEvent(void* object, const devtrace::TraceCaptureProgressEventArgs& args)
{
    auto* self = static_cast<TraceSourceAdapter*>(object);

    RdpCaptureFeatureProgressCallback progress_cb{};
    {
        std::lock_guard lock(self->callback_mutex_);
        progress_cb = self->progress_callback_;
    }

    if (progress_cb.progress_updated == nullptr)
    {
        return;
    }

    const char* progress_text = args.new_progress.progress_text.empty() ? nullptr : args.new_progress.progress_text.c_str();

    RdpCaptureProgressInfo info{};
    info.feature             = self->feature_;
    info.stage               = GetDetailedCaptureStage(args.new_progress.status);
    info.progress            = args.new_progress.progress;
    info.num_bytes_dumped    = args.new_progress.num_bytes_dumped;
    info.total_bytes_to_dump = args.new_progress.total_bytes_to_dump;
    info.progress_text       = progress_text;

    progress_cb.progress_updated(progress_cb.user_data, &info);
}

bool TraceSourceAdapter::Initialize()
{
    if (!conn_manager_->Initialize(trace_source_))
    {
        return false;
    }

    BindToTraceSource();

    return true;
}

void TraceSourceAdapter::BindToTraceSource()
{
    trace_source_->RegisterTraceCompletionEvent({.listener = this, .callback = OnTraceCompletionEvent});
    trace_source_->RegisterStatusEvent({.listener = this, .callback = OnTraceStatusEvent});
    trace_source_->RegisterTraceCaptureProgressEvent({.listener = this, .callback = OnTraceCaptureProgressEvent});
}

RdpCaptureResult TraceSourceAdapter::ConfigureRgp(const RdpCaptureProfilingEnableParams& params)
{
    auto rgp = GetSource<devtrace::RgpTraceSource>();
    if (rgp == nullptr)
    {
        return kRdpCaptureResultFailure;
    }

    devtrace::RgpTraceSourceConfig& config = rgp->GetConfig();

    config.frame_capture_index = params.frame_capture_index;

    if (params.flags & kRdpCaptureProfilingEnableParamFlagUseFrameIndexCapture)
    {
        config.auto_capture_mode = devtrace::AutoCaptureMode::kAutoCaptureModeFrameIndex;
    }
    else if (params.flags & kRdpCaptureProfilingEnableParamFlagUseDispatchIndexCapture)
    {
        // Use timer mode when a delay is specified, otherwise use dispatch indices mode
        if (params.dispatch_capture_delay_ms > 0)
        {
            config.auto_capture_mode = devtrace::AutoCaptureMode::kAutoCaptureModeTimer;
        }
        else
        {
            config.auto_capture_mode = devtrace::AutoCaptureMode::kAutoCaptureModeDispatchIndices;
        }
    }
    else
    {
        config.auto_capture_mode = devtrace::AutoCaptureMode::kAutoCaptureModeNone;
    }

    config.dispatch_start_index         = params.dispatch_start_index;
    config.dispatch_count               = params.dispatch_count;
    config.compute_auto_capture_time_ms = params.dispatch_capture_delay_ms;

    if (getenv("RDP_CAPTURE_API_ENABLE_RGP_LEGACY_CAPTURE") != nullptr)
    {
        //config.enable_legacy_capture = true;
    }

    // Single token SQTT can only be enabled when ubertrace is enabled
    if (getenv("RDP_CAPTURE_API_ENABLE_SINGLE_SQTT_TOKEN") != nullptr && !config.enable_legacy_capture)
    {
        config.enable_single_token_sqtt = true;
    }

    bool enable_shader_inst = (params.flags & kRdpCaptureProfilingEnableParamFlagEnableShaderInstrumentation) != 0;
    return rgp->SetShaderInstrumentationEnabled(enable_shader_inst) == devtrace::Result::kSuccess ? kRdpCaptureResultSuccess : kRdpCaptureResultFailure;
}

RdpCaptureResult TraceSourceAdapter::ConfigureRgd(const RdpCaptureCrashAnalysisEnableParams& params)
{
    auto rgd = GetSource<devtrace::RgdTraceSource>();
    if (rgd == nullptr)
    {
        return kRdpCaptureResultFailure;
    }

    devtrace::RgdTraceSourceConfig& config = rgd->GetConfig();

    const uint32_t flags                  = params.flags;
    const bool     enhanced_crash_enabled = (flags & kRdpCaptureCrashAnalysisEnableParamFlagEnableEnhancedCrashAnalysis) != 0;
    config.enable_advanced_crash          = enhanced_crash_enabled;

    // The GenerateText/JsonSummary bits intentionally do not propagate into the trace source's
    // automatic summary path: the API's registered summary generator is a no-op, so honoring them
    // here would only enqueue a failing processing pass on trace completion. CLI builds spawn
    // rgd[.exe] explicitly after the dump is written instead. Keep the bits defined for future
    // use once a real RgdSummaryGenerator is wired into the API path.
    config.generate_text_summary = false;
    config.generate_json_summary = false;

    devtrace::RgdSummaryOptions summary_options{};
    summary_options.show_marker_source     = (flags & kRdpCaptureCrashAnalysisEnableParamFlagShowMarkerSource) != 0;
    summary_options.expand_markers         = (flags & kRdpCaptureCrashAnalysisEnableParamFlagExpandMarkers) != 0;
    summary_options.pdb_include_subfolders = (flags & kRdpCaptureCrashAnalysisEnableParamFlagPdbIncludeSubfolders) != 0;
    if (params.pdb_search_paths != nullptr && params.num_pdb_search_paths > 0)
    {
        // Defensive cap: this is a public C API surface, so an uninitialized struct
        // or a malicious caller could otherwise force a huge allocation or OOB read.
        constexpr uint64_t kMaxPdbSearchPaths = 1024;
        const uint64_t     count              = std::min<uint64_t>(params.num_pdb_search_paths, kMaxPdbSearchPaths);
        summary_options.pdb_search_paths.reserve(static_cast<size_t>(count));
        for (uint64_t i = 0; i < count; ++i)
        {
            if (params.pdb_search_paths[i] != nullptr)
            {
                summary_options.pdb_search_paths.emplace_back(params.pdb_search_paths[i]);
            }
        }
    }
    config.SetSummaryOptions(summary_options);

    config.collect_wave_sgprs = (flags & kRdpCaptureCrashAnalysisEnableParamFlagCollectWaveSgprs) != 0;
    config.collect_wave_vgprs = (flags & kRdpCaptureCrashAnalysisEnableParamFlagCollectWaveVgprs) != 0;

    return kRdpCaptureResultSuccess;
}

RdpCaptureResult TraceSourceAdapter::ConfigureRra(const RdpCaptureRaytracingEnableParams& params)
{
    auto rra = GetSource<devtrace::RraTraceSource>();
    if (rra == nullptr)
    {
        return kRdpCaptureResultFailure;
    }

    const bool marker_capture_requested = (params.flags & kRdpCaptureRaytracingEnableParamFlagEnableMarkerCapture) != 0;
    if (marker_capture_requested && !rra->GetConfig().is_marker_capture_supported)
    {
        return kRdpCaptureResultUnsupported;
    }

    return kRdpCaptureResultSuccess;
}

void TraceSourceAdapter::SetEnabled(bool enabled)
{
    conn_manager_->SetEnabled(enabled);
}

void TraceSourceAdapter::SetTraceFinishedCallback(const RdpFeatureTraceFinishedCallback& callback)
{
    std::lock_guard lock(callback_mutex_);
    callback_ = callback;
}

void TraceSourceAdapter::SetStatusCallback(const RdpCaptureFeatureStatusCallback& callback)
{
    std::lock_guard lock(callback_mutex_);
    status_callback_ = callback;
}

void TraceSourceAdapter::SetProgressCallback(const RdpCaptureFeatureProgressCallback& callback)
{
    std::lock_guard lock(callback_mutex_);
    progress_callback_ = callback;
}

RdpCaptureFeatureStage TraceSourceAdapter::GetFeatureStage([[maybe_unused]] uint64_t timeout_ms)
{
    return current_stage_;
}

void TraceSourceAdapter::GetFeatureApiConnections(RdpCaptureApiConnection** connections, uint64_t* num_connections)
{
    std::unordered_map<DDConnectionId, devtrace::Api> source_connections = GetTraceSourceConnections();
    *connections     = reinterpret_cast<RdpCaptureApiConnection*>(ApiAlloc(sizeof(RdpCaptureApiConnection) * source_connections.size()));
    *num_connections = source_connections.size();

    size_t index = 0;
    for (auto& pair : source_connections)
    {
        RdpCaptureApiConnection& connection = (*connections)[index++];

        static_assert(sizeof(void*) == sizeof(uint64_t), "Not supported on 32 bit systems");
        uint64_t conn_id = pair.first;

        connection.id  = reinterpret_cast<void*>(conn_id);
        connection.api = GetGpuApi(pair.second);
    }
}

std::unordered_map<DDConnectionId, devtrace::Api> TraceSourceAdapter::GetTraceSourceConnections() const
{
    return connection_map_;
}

uint16_t TraceSourceAdapter::GetConnectionId(RdpCaptureApiConnectionId connection) const
{
    static_assert(sizeof(void*) == sizeof(uint64_t), "Not supported on 32 bit systems");

    if (connection != RDP_CAPTURE_API_FIRST_CONNECTION_ID)
    {
        return static_cast<uint16_t>(reinterpret_cast<uint64_t>(connection));
    }

    std::unordered_map<DDConnectionId, devtrace::Api> current_connections = GetTraceSourceConnections();
    return !current_connections.empty() ? current_connections.begin()->first : 0;
}

RdpCaptureResult TraceSourceAdapter::BeginTrace(RdpCaptureApiConnectionId connection)
{
    if (auto triggerable = GetSource<devtrace::TriggerableTraceSource>())
    {
        return GetCaptureResult(triggerable->RequestBeginTrace(GetConnectionId(connection), begin_trace_capture_mode_));
    }

    return kRdpCaptureResultUnsupported;
}

RdpCaptureResult TraceSourceAdapter::AbortTrace()
{
    const devtrace::TraceSourceStatus status = current_status_;
    if (status.abort_trace_supported)
    {
        // Get the first connection ID to abort
        const DDConnectionId   connection_id      = GetConnectionId(RDP_CAPTURE_API_FIRST_CONNECTION_ID);
        const devtrace::Result abort_trace_result = trace_source_->RequestAbortTrace(connection_id);
        if (abort_trace_result != devtrace::Result::kSuccess)
        {
            return GetCaptureResult(abort_trace_result);
        }
    }

    if (status.abort_processing_supported)
    {
        const devtrace::Result abort_processing_result = trace_source_->RequestAbortProcessing();
        if (abort_processing_result != devtrace::Result::kSuccess)
        {
            return GetCaptureResult(abort_processing_result);
        }
    }

    return kRdpCaptureResultSuccess;
}

RdpCaptureResult TraceSourceAdapter::InsertSnapshot(RdpCaptureApiConnectionId connection, const char* snapshot_name)
{
    if (snapshot_name == nullptr)
    {
        return kRdpCaptureResultInvalidParams;
    }

    if (auto continuous = GetSource<devtrace::ContinuousTraceSource>())
    {
        return GetCaptureResult(continuous->AddMarker(GetConnectionId(connection), snapshot_name));
    }

    return kRdpCaptureResultUnsupported;
}

RdpCaptureResult TraceSourceAdapter::DumpTrace(RdpCaptureApiConnectionId connection)
{
    if (auto continuous = GetSource<devtrace::ContinuousTraceSource>())
    {
        return GetCaptureResult(continuous->RequestDump(GetConnectionId(connection)));
    }

    return kRdpCaptureResultUnsupported;
}

void TraceSourceAdapter::SetProfilingParams(const RdpCaptureProfilingParams* params)
{
    if (params == nullptr)
    {
        return;
    }

    if (const auto rgp = GetSource<devtrace::RgpTraceSource>())
    {
        ApplyProfilingParams(params, rgp);
    }

    begin_trace_capture_mode_ = params->capture_mode;
}

void TraceSourceAdapter::SetRaytracingParams(const RdpCaptureRaytracingParams* params)
{
    if (const auto rra = GetSource<devtrace::RraTraceSource>())
    {
        ApplyRaytracingParams(params, rra);
    }
}
