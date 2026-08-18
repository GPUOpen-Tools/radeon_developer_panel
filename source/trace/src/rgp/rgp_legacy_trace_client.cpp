// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the RGP trace source client.

#include "rgp_legacy_trace_client.h"

#include <utility>

#include "rgp_pal_client_info.h"

/// @brief The profiling timeout value.
static constexpr uint32_t kProfilingTimeout = 10000;

/// @brief A rough estimate for how long a compute dispatch should take.
static constexpr uint32_t kTimeoutEstimatedDispatchTimeMs = 500;

/// @brief The minimum framerate that we expect a graphics application to run at while calculating the timeout.
static constexpr float kTimeoutTargetFramerate = 10.0;

namespace devtrace
{
    void RgpLegacyClient::OnWritingStatusEvent(void* object, const WritingStatusEventArgs& args)
    {
        auto* self = static_cast<RgpLegacyClient*>(object);
        if (args.is_writing)
        {
            self->SetState(ClientState::kDumping);
        }
    }

    RgpLegacyClient::RgpLegacyClient(ClientConnection                              conn_info,
                                     ClientUtils<RgpTraceSourceConfigPrivate>&     client_utils,
                                     DDGpuProfilingApi*                            profiling_api,
                                     const std::shared_ptr<RgpSpmCounterHandler>&  counter_handler,
                                     std::unique_ptr<ActiveGpuProvider>&           active_gpu_provider,
                                     const std::shared_ptr<AdditionalChunkWriter>& additional_chunk_writer)
        : conn_info_(std::move(conn_info))
        , client_utils_(client_utils)
        , profiling_api_(profiling_api)
        , counter_handler_(counter_handler)
        , active_gpu_provider_(std::move(active_gpu_provider))
        , additional_chunk_writer_(additional_chunk_writer)
        , active_gpu_{}
    {
    }

    RgpLegacyClient::~RgpLegacyClient() = default;

    DD_DRIVER_STATE RgpLegacyClient::GetInitDriverState()
    {
        return DD_DRIVER_STATE_DEVICEINIT;
    }

    Result RgpLegacyClient::Initialize()
    {
        const auto& config = client_utils_.GetClientConfig().config;

        // We use some common sense default parameters for the initial config since we don't know what GPU captures are going to be on
        DDGpuProfilingConfig rgp_config{};
        GenerateDdGpuProfilingConfig(
            GetRgpCaptureMode(0, conn_info_.api), client_utils_.GetClientConfig(), kDefaultInstTracingSeMask, false, false, kAutoCaptureModeNone, rgp_config);

        DD_UNUSED(config);
        if (const DD_RESULT result = profiling_api_->EnableTracing(profiling_api_->pInstance, conn_info_.umd_connection_id, &rgp_config);
            result != DD_RESULT_SUCCESS)
        {
            client_utils_.GetLogger()->LogError("Failed to enable tracing [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));

            SetState(ClientState::kError);
            return Result::kFailure;
        }

        client_utils_.GetLogger()->LogInfo("Successfully enabled tracing [{} {}]",
                                           conn_info_.client_pid,
                                           conn_info_.umd_connection_id,
                                           conn_info_.umd_connection_id,
                                           GetHumanReadableName(conn_info_.api));

        chunk_reservation_ = additional_chunk_writer_->Reserve(conn_info_.umd_connection_id);

        return Result::kSuccess;
    }

    void RgpLegacyClient::Disconnect()
    {
        active_gpu_provider_->StopPolling();

        {
            const std::lock_guard abort_lock(abort_mutex_);
            profiling_api_->AbortTrace(profiling_api_->pInstance, conn_info_.umd_connection_id);
        }

        // Join the thread before acquiring the lock since the thread might need to lock state_mutex_ to finish.
        if (capture_thread_.joinable())
        {
            capture_thread_.join();
        }

        additional_chunk_writer_->ReleaseReservation(conn_info_.umd_connection_id, chunk_reservation_);

        profiling_api_->DisableTracing(profiling_api_->pInstance, conn_info_.umd_connection_id);
        SetState(ClientState::kDone);
    }

    Result RgpLegacyClient::HandleDriverState(const DD_DRIVER_STATE state)
    {
        if (state == DD_DRIVER_STATE_POSTDEVICEINIT)
        {
            client_utils_.GetLogger()->LogInfo("Client reached POSTDEVICEINIT state [{} {}]",
                                               conn_info_.client_pid,
                                               conn_info_.umd_connection_id,
                                               conn_info_.umd_connection_id,
                                               GetHumanReadableName(conn_info_.api));
            // We only start polling once the app is running which stops the client from being disabled before the
            // active GPU can be accurately determined.
            active_gpu_provider_->StartPolling({.listener = this, .callback = OnActiveGpuUpdate});

            const RgpTraceSourceConfigPrivate& config = client_utils_.GetClientConfig();

            const AutoCaptureMode auto_capture_mode = config.config.auto_capture_mode;

            if (auto_capture_mode == kAutoCaptureModeFrameIndex && !IsComputeApi(conn_info_.api))
            {
                return RequestAutoCapture(config, true, kAutoCaptureModeNone);
            }

            if (IsComputeApi(conn_info_.api))
            {
                if (auto_capture_mode == kAutoCaptureModeDispatchIndices)
                {
                    return RequestAutoCapture(config, false, auto_capture_mode);
                }

                if (auto_capture_mode == kAutoCaptureModeTimer)
                {
                    return RunTimerCapture(config);
                }
            }
        }

        return Result::kSuccess;
    }

    Result RgpLegacyClient::RequestAutoCapture(const RgpTraceSourceConfigPrivate& config, const bool use_frame_capture, const AutoCaptureMode auto_capture_mode)
    {
        const Result result = RequestBeginTrace(GetRgpCaptureMode(0, conn_info_.api), config, use_frame_capture, auto_capture_mode);
        if (result != Result::kSuccess)
        {
            client_utils_.TraceCompleted(TraceCompletionStatus::kError, "", conn_info_.umd_connection_id);
        }

        return result;
    }

    Result RgpLegacyClient::RunTimerCapture(const RgpTraceSourceConfigPrivate& config)
    {
        // We hold the abort lock so that abort_ can't emit before we subscribe to it.
        const std::lock_guard abort_lock(abort_mutex_);

        if (const Result prepare_result = PrepareForDelayedCapture(); prepare_result != Result::kSuccess)
        {
            return prepare_result;
        }

        timer_.Start(
            config.config.compute_auto_capture_time_ms,
            [&] { RequestAutoCapture(client_utils_.GetClientConfig(), false, kAutoCaptureModeTimer); },
            [&] {
                client_utils_.TraceCompleted(TraceCompletionStatus::kAborted, "", conn_info_.umd_connection_id);
                SetState(ClientState::kDone);
            });

        return Result::kSuccess;
    }

    void RgpLegacyClient::OnActiveGpuUpdate(void* self, const system_info_utils::GpuInfo& gpu)
    {
        if (auto* client = static_cast<RgpLegacyClient*>(self); client->active_gpu_.name != gpu.name)
        {
            client->active_gpu_ = gpu;

            if (const auto current_state = client->GetState(); current_state == ClientState::kIdle || current_state == ClientState::kDisabled)
            {
                const auto new_state = IsGpuSupportedForRgp(client->active_gpu_) ? ClientState::kIdle : ClientState::kDisabled;
                client->SetState(new_state);
            }
        }
    }

    Result RgpLegacyClient::RequestAbortTrace()
    {
        {
            const std::lock_guard abort_lock(abort_mutex_);

            profiling_api_->AbortTrace(profiling_api_->pInstance, conn_info_.umd_connection_id);
            aborted_ = true;
        }

        if (GetState() == ClientState::kWaitingToBeginCapture)
        {
            SetState(ClientState::kIdle);
        }

        return Result::kSuccess;
    }

    bool RgpLegacyClient::IsAbortTraceSupported()
    {
        return true;
    }

    Result RgpLegacyClient::PrepareForDelayedCapture()
    {
        if (const auto current_state = GetState(); current_state != ClientState::kIdle)
        {
            return Result::kNotReady;
        }

        SetState(ClientState::kWaitingToBeginCapture);

        return Result::kSuccess;
    }

    Result RgpLegacyClient::RequestBeginTrace(const uint32_t capture_mode)
    {
        // We don't even need to check the config for auto captures because auto captures will only be invoked by
        // this client, which would call the helper that takes those modes.
        return RequestBeginTrace(GetRgpCaptureMode(capture_mode, conn_info_.api), client_utils_.GetClientConfig(), false, kAutoCaptureModeNone);
    }

    bool RgpLegacyClient::SupportsCaptureMode(const uint32_t mode) const
    {
        const bool           is_compute = IsComputeApi(conn_info_.api);
        const RgpCaptureMode rgp_mode   = GetRgpCaptureMode(mode, conn_info_.api);

        return (is_compute && rgp_mode == RgpCaptureMode::kDispatch) || (!is_compute && rgp_mode == RgpCaptureMode::kFrame);
    }

    Result RgpLegacyClient::RequestBeginTrace(RgpCaptureMode                     capture_mode,
                                              const RgpTraceSourceConfigPrivate& config,
                                              const bool                         use_frame_capture,
                                              const AutoCaptureMode              auto_capture_mode)
    {
        if (!SupportsCaptureMode(static_cast<uint32_t>(capture_mode)))
        {
            return Result::kUnsupported;
        }

        if (!IsReadyForCapture())
        {
            return Result::kNotReady;
        }

        if (capture_thread_.joinable())
        {
            capture_thread_.join();
        }

        DDGpuProfilingConfig  rgp_config{};
        SpmCounterQueryResult spm_counters{};

        // We always choose to get the most up-to-date GPU, even though we are polling periodically just in case.
        system_info_utils::GpuInfo active_gpu   = active_gpu_provider_->QueryActiveGpu();
        const bool                 supports_spm = active_gpu_provider_->DoesGpuSupportSpm(active_gpu);

        const bool is_auto_capture = use_frame_capture || auto_capture_mode != kAutoCaptureModeNone;
        if (!IsGpuSupportedForRgp(active_gpu))
        {
            client_utils_.GetLogger()->LogError("Failed to take RGP capture because GPU ({}) was not supported [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                active_gpu.name,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));

            if (is_auto_capture)
            {
                SetState(ClientState::kDone);
            }

            return Result::kUnsupported;
        }

        const uint32_t inst_tracing_mask = GetSeMaskForInstructionTracing(active_gpu.asic.cu_mask);

        GenerateDdGpuProfilingConfig(capture_mode, config, inst_tracing_mask, supports_spm, use_frame_capture, auto_capture_mode, rgp_config);

        if (rgp_config.flags.enableSpm == 1)
        {
            if (const Result spm_result = UpdateSpmCounters(config.config, active_gpu, spm_counters); spm_result != Result::kSuccess)
            {
                if (is_auto_capture)
                {
                    SetState(ClientState::kDone);
                }

                return spm_result;
            }
        }

        const uint32_t timeout_ms = GetTimeout(capture_mode, config.config, rgp_config, use_frame_capture, auto_capture_mode);
        capture_thread_           = std::thread([=, this] { ExecuteTrace(rgp_config, spm_counters, timeout_ms, is_auto_capture); });

        return Result::kSuccess;
    }

    Result RgpLegacyClient::UpdateSpmCounters([[maybe_unused]] const RgpTraceSourceConfig& config,
                                              const system_info_utils::GpuInfo&            gpu,
                                              SpmCounterQueryResult&                       query_result)
    {
        query_result.derived_groups   = {};
        query_result.derived_counters = {};
        if (query_result.derived_counters.empty())
        {
            counter_handler_->DefaultCounters(gpu, query_result.derived_groups, query_result.derived_counters);
        }

        if (counter_handler_->QuerySpmCounters(gpu, query_result) != Result::kSuccess)
        {
            client_utils_.GetLogger()->LogError("Failed to query SPM counters [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));

            return Result::kFailure;
        }

        client_utils_.GetLogger()->LogInfo("Successfully queried SPM counters [{} {}]",
                                           conn_info_.client_pid,
                                           conn_info_.umd_connection_id,
                                           conn_info_.umd_connection_id,
                                           GetHumanReadableName(conn_info_.api));

        const auto& counter_list = query_result.hardware_counters;
        const auto  num_counters = static_cast<uint32_t>(counter_list.size());

        if (const DD_RESULT result = profiling_api_->SetSpmCounters(profiling_api_->pInstance, conn_info_.umd_connection_id, counter_list.data(), num_counters);
            result != DD_RESULT_SUCCESS)
        {
            client_utils_.GetLogger()->LogError("Failed to update SPM counters [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));

            return Result::kFailure;
        }

        client_utils_.GetLogger()->LogInfo("Successfully updated SPM counters [{} {}]",
                                           conn_info_.client_pid,
                                           conn_info_.umd_connection_id,
                                           conn_info_.umd_connection_id,
                                           GetHumanReadableName(conn_info_.api));

        return Result::kSuccess;
    }

    void RgpLegacyClient::ExecuteTrace(const DDGpuProfilingConfig&  config,
                                       const SpmCounterQueryResult& spm_counters,
                                       const uint32_t               timeout,
                                       const bool                   is_auto_capture)
    {
        DD_RESULT result;
        auto      additional_chunk_result = Result::kSuccess;
        aborted_                          = false;

        std::string path;
        {
            const std::unique_ptr<ByteWriter> writer = client_utils_.GetByteWriter(path);
            writer->RegisterWritingStatusEvent({.listener = this, .callback = &OnWritingStatusEvent});

            writer->SetPostProcessOperation([&](const std::unique_ptr<ReadWriteStream>& stream) {
                additional_chunk_result = additional_chunk_writer_->WriteLegacyRgp(conn_info_.umd_connection_id, stream);
            });

            DDGpuProfilingTraceArgs args{};
            args.config      = config;
            args.timeoutInMs = timeout;
            args.writer      = writer->Writer();

            args.pPostBeginTraceCallback = &RgpLegacyClient::OnBeginTrace;
            args.pPostBeginTraceUserdata = this;

            result = profiling_api_->ExecuteTrace(profiling_api_->pInstance, conn_info_.umd_connection_id, &args);
        }

        FinalizeTrace(result, additional_chunk_result, path, config, spm_counters, is_auto_capture, aborted_);
    }

    void RgpLegacyClient::RegisterSpmTraceListener(const SpmTraceEvent& event)
    {
        spm_trace_event_ = event;
    }

    void RgpLegacyClient::FinalizeTrace(const DD_RESULT              result,
                                        const Result                 additional_chunk_result,
                                        const std::string&           path,
                                        const DDGpuProfilingConfig&  config,
                                        const SpmCounterQueryResult& spm_counters,
                                        const bool                   is_auto_capture,
                                        const bool                   was_aborted)
    {
        if (result == DD_RESULT_SUCCESS)
        {
            client_utils_.GetLogger()->LogInfo("Successfully captured trace [{} {}]",
                                               conn_info_.client_pid,
                                               conn_info_.umd_connection_id,
                                               conn_info_.umd_connection_id,
                                               GetHumanReadableName(conn_info_.api));

            if (additional_chunk_result == Result::kSuccess)
            {
                client_utils_.GetLogger()->LogInfo("Successfully wrote additional chunks [{} {}]",
                                                   conn_info_.client_pid,
                                                   conn_info_.umd_connection_id,
                                                   conn_info_.umd_connection_id,
                                                   GetHumanReadableName(conn_info_.api));

                if (config.flags.enableSpm && spm_trace_event_.on_trace_completed != nullptr)
                {
                    SetState(ClientState::kPostProcessing);
                    client_utils_.TraceCompleted(TraceCompletionStatus::kNeedProcessing, path, conn_info_.umd_connection_id);

                    const SpmTrace spm_trace = {.path = path, .counters = spm_counters, .is_rdf = false, .umd_connection_id = conn_info_.umd_connection_id};
                    spm_trace_event_.on_trace_completed(spm_trace_event_.listener, spm_trace);
                }
                else
                {
                    client_utils_.TraceCompleted(TraceCompletionStatus::kCompleted, path, conn_info_.umd_connection_id);
                }
            }
            else
            {
                client_utils_.GetLogger()->LogError("Failed to write additional chunks [{} {}]",
                                                    conn_info_.client_pid,
                                                    conn_info_.umd_connection_id,
                                                    conn_info_.umd_connection_id,
                                                    GetHumanReadableName(conn_info_.api));

                client_utils_.TraceCompleted(TraceCompletionStatus::kError, path, conn_info_.umd_connection_id);
            }
        }
        else if (!was_aborted)
        {
            client_utils_.GetLogger()->LogError("Failed to capture trace [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));

            client_utils_.TraceCompleted(TraceCompletionStatus::kError, path, conn_info_.umd_connection_id);
        }
        else
        {
            client_utils_.GetLogger()->LogInfo("Trace was aborted [{} {}]",
                                               conn_info_.client_pid,
                                               conn_info_.umd_connection_id,
                                               conn_info_.umd_connection_id,
                                               GetHumanReadableName(conn_info_.api));

            client_utils_.TraceCompleted(TraceCompletionStatus::kAborted, path, conn_info_.umd_connection_id);
        }

        if (is_auto_capture)
        {
            SetState(ClientState::kDone);
        }
        else
        {
            const auto new_state = IsGpuSupportedForRgp(active_gpu_) ? ClientState::kIdle : ClientState::kDisabled;
            SetState(new_state);
        }
    }

    void RgpLegacyClient::OnBeginTrace(void* userdata)
    {
        auto* client = static_cast<RgpLegacyClient*>(userdata);

        client->SetState(ClientState::kCapturing);
        client->client_utils_.GetLogger()->LogInfo("Successfully began trace [{} {}]",
                                                   client->conn_info_.client_pid,
                                                   client->conn_info_.umd_connection_id,
                                                   client->conn_info_.umd_connection_id,
                                                   GetHumanReadableName(client->conn_info_.api));

        // Emit initial indeterminate progress
        WritingProgress initial_progress{};
        initial_progress.progress = 0.0F;  // Indeterminate progress bar
        client->client_utils_.ReportCaptureProgress(initial_progress);
    }

    void RgpLegacyClient::GenerateDdGpuProfilingConfig(const RgpCaptureMode               capture_mode,
                                                       const RgpTraceSourceConfigPrivate& config,
                                                       const uint32_t                     inst_tracing_mask,
                                                       [[maybe_unused]] const bool        spm_supported,
                                                       [[maybe_unused]] const bool        use_frame_capture,
                                                       const AutoCaptureMode              auto_capture_mode,
                                                       DDGpuProfilingConfig&              rgp_config)
    {
        rgp_config.gpuMemoryLimitInMb   = config.config.sqtt_memory_limit;
        rgp_config.numPreparationFrames = kNumPreparationFrames;

        rgp_config.flags.enableInstructionTokens  = config.inst_tracing_supported && config.config.enable_inst_tracing;
        rgp_config.flags.allowComputePresents     = 0;
        rgp_config.flags.captureDriverCodeObjects = 0;
        rgp_config.flags.enableSpm                = spm_supported && config.config.enable_spm_counters;

        rgp_config.instructionTraceApiPsoHash       = 0;
        rgp_config.shaderEngineInstructionTraceMask = inst_tracing_mask;

        rgp_config.spmMemoryLimit = kSpmMemoryLimit;

        rgp_config.spmSampleFrequency = kDefaultSpmSamplingFreq;

        if (capture_mode == RgpCaptureMode::kFrame)
        {
            rgp_config.captureMode = DD_GPU_PROFILING_TRIGGER_MODE_PRESENT;
            if (use_frame_capture)
            {
                rgp_config.captureMode = DD_GPU_PROFILING_TRIGGER_MODE_FRAME_INDEX;

                rgp_config.captureStartIndex = config.config.frame_capture_index;
                rgp_config.captureStopIndex  = rgp_config.captureStartIndex + 1;
            }
        }
        else
        {
            rgp_config.captureMode       = DD_GPU_PROFILING_TRIGGER_MODE_DISPATCH_INDEX;
            rgp_config.captureStartIndex = auto_capture_mode == kAutoCaptureModeDispatchIndices ? config.config.dispatch_start_index.load() : 0;
            rgp_config.captureStopIndex  = rgp_config.captureStartIndex + config.config.dispatch_count;
        }
    }

    uint32_t RgpLegacyClient::GetTimeout(const RgpCaptureMode                         capture_mode,
                                         [[maybe_unused]] const RgpTraceSourceConfig& config,
                                         const DDGpuProfilingConfig&                  rgp_config,
                                         const bool                                   use_frame_capture,
                                         const AutoCaptureMode                        auto_capture_mode)
    {
        if (capture_mode == RgpCaptureMode::kDispatch)
        {
            if (auto_capture_mode == kAutoCaptureModeDispatchIndices)
            {
                // Compute apps tend to be quite unpredictable, so we just disable the timeout for auto capture since
                // there isn't really a good way to estimate the time it will take
                return static_cast<uint32_t>(-1);
            }

            // The time to take regular captures can be estimated by assuming that a dispatch takes some time (kTimeoutEstimatedDispatchTimeMs) and then figuring out
            // how much time is needed to capture that number of dispatches.
            const uint32_t dispatch_count = rgp_config.captureStopIndex - rgp_config.captureStartIndex;
            return std::max(dispatch_count * kTimeoutEstimatedDispatchTimeMs, kProfilingTimeout);
        }

        // For graphics applications we assume that the application will run at some low framerate (kTimeoutTargetFramerate)
        // and calculate the number of milliseconds it would take for it running at that framerate to capture a profile.
        if (use_frame_capture)
        {
            const float    estimated_seconds      = static_cast<float>(rgp_config.captureStartIndex) / kTimeoutTargetFramerate;
            const uint32_t estimated_milliseconds = static_cast<uint32_t>(std::ceil(estimated_seconds)) * 1000;

            return std::max(estimated_milliseconds, kProfilingTimeout);
        }

        return kProfilingTimeout;
    }

    bool RgpLegacyClient::IsReadyForCapture()
    {
        const auto current_state = GetState();
        return current_state == ClientState::kIdle || current_state == ClientState::kWaitingToBeginCapture;
    }

}  // namespace devtrace
