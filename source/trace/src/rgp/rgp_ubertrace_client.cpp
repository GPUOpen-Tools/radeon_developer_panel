// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the Ubertrace RGP trace source client.

#include "rgp_ubertrace_client.h"

#include "device_clocks.h"

namespace devtrace
{
    RgpUbertraceClient::RgpUbertraceClient(const ClientConnection&                       conn_info,
                                           ClientUtils<RgpTraceSourceConfigPrivate>&     client_utils,
                                           std::unique_ptr<UbertraceUser>                user,
                                           const std::shared_ptr<AdditionalChunkWriter>& addl_chunk_writer,
                                           std::unique_ptr<ActiveGpuProvider>&           active_gpu_provider,
                                           const std::shared_ptr<RgpSpmCounterHandler>&  counter_handler,
                                           const std::shared_ptr<DeviceClocksManager>&   device_clocks_manager)
        : UbertraceClient(conn_info, client_utils, std::move(user), addl_chunk_writer, true)
        , active_gpu_provider_(std::move(active_gpu_provider))
        , counter_handler_(counter_handler)
        , device_clocks_manager_(device_clocks_manager)
        , spm_trace_event_()
    {
    }

    void RgpUbertraceClient::GetEarlyConfiguration(const RgpTraceSourceConfigPrivate& config, UberTraceConfig& early_config)
    {
        // Send early global parameters if single token SQTT is enabled
        if (config.config.enable_single_token_sqtt)
        {
            early_config.global_params                                  = std::make_unique<UberTraceGlobalParams>();
            early_config.global_params->enable_single_token_sqtt_write_ = true;
        }
    }

    void RgpUbertraceClient::Disconnect()
    {
        return UbertraceClient::Disconnect();
    }

    Result RgpUbertraceClient::HandleDriverState(const DD_DRIVER_STATE state)
    {
        if (state == DD_DRIVER_STATE_POSTDEVICEINIT)
        {
            // Check if the active GPU is supported for RGP.
            const auto gpu = active_gpu_provider_->QueryActiveGpu();
            SetState(IsGpuSupportedForRgp(gpu) ? ClientState::kIdle : ClientState::kDisabled);
        }

        return UbertraceClient::HandleDriverState(state);
    }

    void RgpUbertraceClient::OnActiveGpuUpdate(void* self, const system_info_utils::GpuInfo& gpu)
    {
        auto* client        = static_cast<RgpUbertraceClient*>(self);
        client->active_gpu_ = gpu;

        if (client->state_ == ClientState::kIdle || client->state_ == ClientState::kDisabled)
        {
            client->SetState(IsGpuSupportedForRgp(client->active_gpu_) ? ClientState::kIdle : ClientState::kDisabled);
        }
    }

    std::optional<UbertraceAutoCaptureConfig> RgpUbertraceClient::GetAutoCaptureConfig(const RgpTraceSourceConfigPrivate& config)
    {
        if (const AutoCaptureMode mode = config.config.auto_capture_mode; mode != kAutoCaptureModeNone)
        {
            if (mode == kAutoCaptureModeFrameIndex)
            {
                return UbertraceAutoCaptureConfig{0, static_cast<uint32_t>(RgpCaptureMode::kFrame)};
            }

            if (mode == kAutoCaptureModeDispatchIndices)
            {
                return UbertraceAutoCaptureConfig{0, static_cast<uint32_t>(RgpCaptureMode::kDispatch)};
            }

            if (mode == kAutoCaptureModeTimer)
            {
                return UbertraceAutoCaptureConfig{config.config.compute_auto_capture_time_ms, static_cast<uint32_t>(RgpCaptureMode::kDispatch)};
            }
        }

        return {};
    }

    Result RgpUbertraceClient::GenerateCaptureConfig(const RgpTraceSourceConfigPrivate& config, UbertraceCaptureConfig& capture_config)
    {
        RgpCaptureMode rgp_mode = GetRgpCaptureMode(capture_config.capture_mode, GetConnInfo().api);
        if (!SupportsCaptureMode(static_cast<uint32_t>(rgp_mode)))
        {
            return Result::kUnsupported;
        }

        const auto gpu = active_gpu_provider_->QueryActiveGpu();
        if (!IsGpuSupportedForRgp(gpu))
        {
            GetLogger()->LogError("Failed to take RGP capture because GPU ({}) was not supported [{} {}]",
                                  GetConnInfo().client_pid,
                                  GetConnInfo().umd_connection_id,
                                  gpu.name,
                                  GetConnInfo().umd_connection_id,
                                  GetHumanReadableName(GetConnInfo().api));

            return Result::kUnsupported;
        }

        UberTraceSource asic_info_src{};
        asic_info_src.name   = "asicinfo";
        asic_info_src.config = std::make_shared<UbertraceSourceConfig>();

        UberTraceSource api_info_src{};
        api_info_src.name   = "apiinfo";
        api_info_src.config = std::make_shared<UbertraceSourceConfig>();

        UberTraceSource clock_calibration{};
        clock_calibration.name   = "clockcalibration";
        clock_calibration.config = std::make_shared<UbertraceSourceConfig>();

        UberTraceSource code_object{};
        code_object.name   = "codeobject";
        code_object.config = std::make_shared<UbertraceSourceConfig>();

        UberTraceSource queue_timings{};
        queue_timings.name   = "queuetimings";
        queue_timings.config = std::make_shared<UbertraceSourceConfig>();

        UberTraceSource trace_config{};
        trace_config.name   = "traceconfig";
        trace_config.config = std::make_shared<UbertraceSourceConfig>();

        UberTraceSource gpu_perf{};
        gpu_perf.name = "gpuperfexp";

        if (const Result gpu_perf_result = GetGpuPerfConfig(config, gpu, gpu_perf.config, capture_config.on_completed); gpu_perf_result != Result::kSuccess)
        {
            return gpu_perf_result;
        }

        capture_config.ubertrace_config.controllers =
            std::make_unique<UbertraceControllerCollection>(GetFeatures(), GenerateController(rgp_mode, capture_config.capture_type, config));

        // Build the sources list - stringtable, usermarkerhist, and sqttinstrumentation are only needed for single token SQTT
        std::vector sources = {asic_info_src, api_info_src, clock_calibration, code_object, gpu_perf, queue_timings, trace_config};

        if (config.config.enable_single_token_sqtt)
        {
            UberTraceSource strtbl{};
            strtbl.name   = "stringtable";
            strtbl.config = std::make_shared<UbertraceSourceConfig>();

            UberTraceSource user_marker{};
            user_marker.name   = "usermarkerhist";
            user_marker.config = std::make_shared<UbertraceSourceConfig>();

            UberTraceSource sqtt_instrumentation{};
            sqtt_instrumentation.name   = "sqttinstrumentation";
            sqtt_instrumentation.config = std::make_shared<UbertraceSourceConfig>();

            sources.push_back(strtbl);
            sources.push_back(user_marker);
            sources.push_back(sqtt_instrumentation);
        }

        capture_config.ubertrace_config.sources = sources;

        return Result::kSuccess;
    }

    Result RgpUbertraceClient::GetGpuPerfConfig(const RgpTraceSourceConfigPrivate&       config,
                                                const system_info_utils::GpuInfo&        gpu,
                                                std::shared_ptr<UbertraceSourceConfig>&  gpu_perf_config,
                                                std::function<void(const std::string&)>& on_trace_completed)
    {
        uint32_t instruction_tracing_mask = GetSeMaskForInstructionTracing(gpu.asic.cu_mask);
        if (!config.config.enable_spm_counters || !DoesGpuSupportsSpm(config.spm_supported_gpus, gpu))
        {
            gpu_perf_config = std::make_shared<UberTraceGpuPerfSourceConfig>(
                true, config.config.sqtt_memory_limit, config.config.enable_inst_tracing, config.config.enable_exec_pop_tokens, instruction_tracing_mask);

            return Result::kSuccess;
        }

        SpmCounterQueryResult counters;
        if (QuerySpmCounters(config, gpu, counters) != Result::kSuccess)
        {
            return Result::kFailure;
        }

        uint32_t sample_freq = kDefaultSpmSamplingFreq;

        gpu_perf_config = std::make_shared<UberTraceGpuPerfSpmSourceConfig>(true,
                                                                            config.config.sqtt_memory_limit,
                                                                            config.config.enable_inst_tracing,
                                                                            config.config.enable_exec_pop_tokens,
                                                                            instruction_tracing_mask,
                                                                            counters.hardware_counters,
                                                                            sample_freq,
                                                                            kSpmMemoryLimit);

        on_trace_completed = [&, counters = std::move(counters)](const std::string& path) {
            if (config.config.enable_spm_counters && spm_trace_event_.on_trace_completed != nullptr)
            {
                const SpmTrace trace{
                    .path              = path,
                    .counters          = counters,
                    .is_rdf            = true,
                    .umd_connection_id = GetConnInfo().umd_connection_id,
                };
                spm_trace_event_.on_trace_completed(spm_trace_event_.listener, trace);
            }
        };

        return Result::kSuccess;
    }

    Result RgpUbertraceClient::QuerySpmCounters([[maybe_unused]] const RgpTraceSourceConfigPrivate& config,
                                                const system_info_utils::GpuInfo&                   gpu,
                                                SpmCounterQueryResult&                              counters)
    {
        counters.derived_groups   = {};
        counters.derived_counters = {};
        if (counters.derived_counters.empty())
        {
            counter_handler_->DefaultCounters(gpu, counters.derived_groups, counters.derived_counters);
        }

        if (const Result result = counter_handler_->QuerySpmCounters(gpu, counters); result != Result::kSuccess)
        {
            GetLogger()->LogError("Failed to query SPM counters [{} {}]",
                                  GetConnInfo().client_pid,
                                  GetConnInfo().umd_connection_id,
                                  GetConnInfo().umd_connection_id,
                                  GetHumanReadableName(GetConnInfo().api));

            return result;
        }

        GetLogger()->LogInfo("Successfully queried SPM counters [{} {}]",
                             GetConnInfo().client_pid,
                             GetConnInfo().umd_connection_id,
                             GetConnInfo().umd_connection_id,
                             GetHumanReadableName(GetConnInfo().api));

        return Result::kSuccess;
    }

    UbertraceController RgpUbertraceClient::GenerateController(const RgpCaptureMode               capture_mode,
                                                               const UberTraceCaptureType         capture_type,
                                                               const RgpTraceSourceConfigPrivate& config) const
    {
        const bool        is_std_auto_capture     = capture_type == kUberTraceCaptureTypeAutoCapture;
        const std::string controller_capture_mode = is_std_auto_capture ? "absolute" : "relative";

        const UbertraceFeatures& features = GetFeatures();
        if (capture_mode == RgpCaptureMode::kFrame)
        {
            const uint32_t prep_start_index = is_std_auto_capture ? config.config.frame_capture_index - kNumPreparationFrames - 1 : 0;
            return features.GetFrameController(true, kNumPreparationFrames, 1, controller_capture_mode, prep_start_index);
        }

        std::string render_op_mode;
        uint32_t    capture_render_op_count = 1;

        switch (capture_mode)
        {
        case RgpCaptureMode::kDraw:
            render_op_mode          = "draw";
            capture_render_op_count = config.config.draw_count;
            break;
        case RgpCaptureMode::kDispatch:
            render_op_mode          = "dispatch";
            capture_render_op_count = config.config.dispatch_count;
            break;
        default:
            break;
        }

        UbertraceController controller{};
        controller.name = "renderop";

        const uint32_t prep_start_index = is_std_auto_capture ? config.config.dispatch_start_index.load() : 0;
        controller.config =
            std::make_shared<UbertraceRenderOpControllerConfig>(true, render_op_mode, 0, capture_render_op_count, controller_capture_mode, prep_start_index);

        return controller;
    }

    bool RgpUbertraceClient::SupportsCaptureMode(const uint32_t mode) const
    {
        const Api            api        = GetConnInfo().api;
        const bool           is_compute = IsComputeApi(api);
        const RgpCaptureMode rgp_mode   = GetRgpCaptureMode(mode, api);

        if (is_compute)
        {
            return rgp_mode == RgpCaptureMode::kDispatch;
        }

        if (rgp_mode == RgpCaptureMode::kFrame)
        {
            return true;
        }

        switch (rgp_mode)
        {
        case RgpCaptureMode::kFrame:
        case RgpCaptureMode::kDraw:
        case RgpCaptureMode::kDispatch:
            return true;
        default:
            break;
        }

        return false;
    }

    void RgpUbertraceClient::TracingStarted()
    {
        if (!device_clocks_manager_->SetForcePeak(GetConnInfo().umd_connection_id, true))
        {
            GetLogger()->LogWarning("Unable to set clocks to peak [{} {}]",
                                    GetConnInfo().client_pid,
                                    GetConnInfo().umd_connection_id,
                                    GetConnInfo().umd_connection_id,
                                    GetHumanReadableName(GetConnInfo().api));

            return;
        }

        GetLogger()->LogInfo("Successfully set clocks to peak [{} {}]",
                             GetConnInfo().client_pid,
                             GetConnInfo().umd_connection_id,
                             GetConnInfo().umd_connection_id,
                             GetHumanReadableName(GetConnInfo().api));
    }

    void RgpUbertraceClient::TracingEnded()
    {
        if (!device_clocks_manager_->SetForcePeak(GetConnInfo().umd_connection_id, false))
        {
            GetLogger()->LogWarning("Unable to restore clocks [{} {}]",
                                    GetConnInfo().client_pid,
                                    GetConnInfo().umd_connection_id,
                                    GetConnInfo().umd_connection_id,
                                    GetHumanReadableName(GetConnInfo().api));

            return;
        }

        GetLogger()->LogInfo("Successfully restored clocks [{} {}]",
                             GetConnInfo().client_pid,
                             GetConnInfo().umd_connection_id,
                             GetConnInfo().umd_connection_id,
                             GetHumanReadableName(GetConnInfo().api));
    }

    void RgpUbertraceClient::RegisterSpmTraceListener(const SpmTraceEvent& event)
    {
        spm_trace_event_ = event;
    }

}  // namespace devtrace
