// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD DeveloperTools Team
/// @file
/// @brief Implementation for the main crash analysis client.

#include "rgd_client.h"

#include <utility>

static constexpr uint32_t kUberTraceWaitTimeMs = 5000;

namespace devtrace
{
    void RgdClient::OnWritingStatusEvent(void* object, const WritingStatusEventArgs& args)
    {
        auto* self = static_cast<RgdClient*>(object);
        if (args.is_writing)
        {
            self->SetState(ClientState::kDumping);
        }
    }

    RgdClient::RgdClient(ClientConnection                              conn_info,
                         ClientUtils<RgdTraceSourceConfig>&            client_utils,
                         DDGpuDetectiveApi*                            gpu_detective_api,
                         DDEnhancedCrashInfoApi*                       enhanced_api,
                         std::unique_ptr<RgdUbertraceClient>           ubertrace_client,
                         const std::shared_ptr<AdditionalChunkWriter>& additional_chunk_writer)
        : conn_info_(std::move(conn_info))
        , gpu_detective_api_(gpu_detective_api)
        , enhanced_api_(enhanced_api)
        , ubertrace_client_(std::move(ubertrace_client))
        , additional_chunk_writer_(additional_chunk_writer)
        , client_utils_(client_utils)
        , process_info_chunk_()
    {
    }

    RgdClient::~RgdClient() = default;

    DD_DRIVER_STATE RgdClient::GetInitDriverState()
    {
        return DD_DRIVER_STATE_PLATFORMINIT;
    }

    Result RgdClient::Initialize()
    {
        if (ubertrace_client_ != nullptr)
        {
            if (ubertrace_client_->Initialize() != Result::kSuccess)
            {
                client_utils_.GetLogger()->LogError("Failed to initialize UberTrace [{} {}]",
                                                    conn_info_.client_pid,
                                                    conn_info_.umd_connection_id,
                                                    conn_info_.umd_connection_id,
                                                    GetHumanReadableName(conn_info_.api));

                SetState(ClientState::kError);
                return Result::kFailure;
            }

            client_utils_.GetLogger()->LogInfo("Successfully initialized UberTrace [{} {}]",
                                               conn_info_.client_pid,
                                               conn_info_.umd_connection_id,
                                               conn_info_.umd_connection_id,
                                               GetHumanReadableName(conn_info_.api));
        }

        DD_RESULT result = HandleHardwareCrashAnalysis();
        if (result != DD_RESULT_SUCCESS)
        {
            client_utils_.GetLogger()->LogError("Failed to update enhanced crash info config [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));

            SetState(ClientState::kError);
            return Result::kFailure;
        }

        result = gpu_detective_api_->EnableTracing(gpu_detective_api_->pInstance, conn_info_.umd_connection_id, conn_info_.client_pid);
        if (result != DD_RESULT_SUCCESS)
        {
            client_utils_.GetLogger()->LogError("Failed to initialize crash analysis [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));

            SetState(ClientState::kError);
            return Result::kFailure;
        }

        client_utils_.GetLogger()->LogInfo("Successfully initialized crash analysis [{} {}]",
                                           conn_info_.client_pid,
                                           conn_info_.umd_connection_id,
                                           conn_info_.umd_connection_id,
                                           GetHumanReadableName(conn_info_.api));

        SetState(ClientState::kCapturing);

        chunk_reservation_  = additional_chunk_writer_->Reserve(conn_info_.umd_connection_id);
        process_info_chunk_ = additional_chunk_writer_->PrepareProcessInfoChunk(conn_info_.client_pid);

        return Result::kSuccess;
    }

    DD_RESULT RgdClient::HandleHardwareCrashAnalysis()
    {
        const bool enable_hardware_crash = client_utils_.GetClientConfig().enable_advanced_crash;
        if (!enable_hardware_crash)
        {
            extended_info_chunk_.SetEnhancedCrashConfig({});
            return DD_RESULT_SUCCESS;
        }

        const bool collect_wave_sgprs = client_utils_.GetClientConfig().collect_wave_sgprs;
        const bool collect_wave_vgprs = client_utils_.GetClientConfig().collect_wave_vgprs;

        DDEnhancedCrashInfoConfig enhanced_config{};
        enhanced_config.processId             = conn_info_.client_pid;
        enhanced_config.flags.captureWaveData = 1;
        enhanced_config.flags.captureSGPRData = collect_wave_sgprs;
        enhanced_config.flags.captureVGPRData = collect_wave_vgprs;

        // Public builds always have single mem and alu ops enabled
        enhanced_config.flags.enableSingleMemOp = 1;
        enhanced_config.flags.enableSingleAluOp = 1;

        client_utils_.GetLogger()->LogInfo(
            "Applying enhanced crash config: EnableHardwareCrash={}, CaptureSGPRData={}, CaptureVGPRData={}, EnableSingleMemOp={}, EnableSingleAluOp={}",
            conn_info_.client_pid,
            conn_info_.umd_connection_id,
            enable_hardware_crash,
            collect_wave_sgprs,
            collect_wave_vgprs,
            static_cast<bool>(enhanced_config.flags.enableSingleMemOp),
            static_cast<bool>(enhanced_config.flags.enableSingleAluOp));

        extended_info_chunk_.SetEnhancedCrashConfig(ExtendedInfoChunkHardwareCrashInfoConfig{enable_hardware_crash, enhanced_config});
        return enhanced_api_->SetEnhancedCrashInfoConfig(enhanced_api_->pInstance, &enhanced_config);
    }

    void RgdClient::Disconnect()
    {
        client_utils_.GetLogger()->LogInfo("Disconnecting client [{} {}]",
                                           conn_info_.client_pid,
                                           conn_info_.umd_connection_id,
                                           conn_info_.umd_connection_id,
                                           GetHumanReadableName(conn_info_.api));

        bool       did_detect_crash = false;
        const auto result =
            gpu_detective_api_->EndTracing(gpu_detective_api_->pInstance, conn_info_.umd_connection_id, has_reached_post_device_init_, &did_detect_crash);
        if (result != DD_RESULT_SUCCESS)
        {
            client_utils_.GetLogger()->LogError("Failed to end tracing [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));
        }
        else
        {
            client_utils_.GetLogger()->LogInfo("Ended tracing [{} {}]",
                                               conn_info_.client_pid,
                                               conn_info_.umd_connection_id,
                                               conn_info_.umd_connection_id,
                                               GetHumanReadableName(conn_info_.api));
        }

        if (ubertrace_client_)
        {
            ubertrace_client_->Disconnect();
        }

        if (did_detect_crash)
        {
            client_utils_.GetLogger()->LogInfo("Detected crash [{} {}]",
                                               conn_info_.client_pid,
                                               conn_info_.umd_connection_id,
                                               conn_info_.umd_connection_id,
                                               GetHumanReadableName(conn_info_.api));

            DumpCrash();
        }
        else
        {
            client_utils_.GetLogger()->LogInfo("Crash not detected [{} {}]",
                                               conn_info_.client_pid,
                                               conn_info_.umd_connection_id,
                                               conn_info_.umd_connection_id,
                                               GetHumanReadableName(conn_info_.api));

            PostNoCrashDetectedEvent();
        }

        SetState(ClientState::kDone);

        gpu_detective_api_->DisableTracing(gpu_detective_api_->pInstance, conn_info_.umd_connection_id);
        additional_chunk_writer_->ReleaseReservation(conn_info_.umd_connection_id, chunk_reservation_);
    }

    Result RgdClient::HandleDriverState(const DD_DRIVER_STATE state)
    {
        if (ubertrace_client_ != nullptr)
        {
            if (const Result ubertrace_result = ubertrace_client_->HandleDriverState(state); ubertrace_result != Result::kSuccess)
            {
                return ubertrace_result;
            }
        }

        if (state == DD_DRIVER_STATE_POSTDEVICEINIT)
        {
            has_reached_post_device_init_ = true;
        }

        if (state != DD_DRIVER_STATE_RUNNING)
        {
            return Result::kSuccess;
        }

        if (ubertrace_client_ == nullptr)
        {
            return Result::kSuccess;
        }

        if (ubertrace_client_->RequestBeginTrace(0) != Result::kSuccess)
        {
            client_utils_.GetLogger()->LogError("Failed to request UberTrace [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));

            SetState(ClientState::kError);
            return Result::kFailure;
        }

        client_utils_.GetLogger()->LogInfo("Successfully requested UberTrace [{} {}]",
                                           conn_info_.client_pid,
                                           conn_info_.umd_connection_id,
                                           conn_info_.umd_connection_id,
                                           GetHumanReadableName(conn_info_.api));

        return Result::kSuccess;
    }

    Result RgdClient::RequestAbortTrace()
    {
        if (!IsAbortTraceSupported())
        {
            return Result::kUnsupported;
        }

        return Result::kSuccess;
    }

    bool RgdClient::IsAbortTraceSupported()
    {
        return false;
    }

    Result RgdClient::RequestDump()
    {
        return Result::kUnsupported;
    }
    Result RgdClient::AddMarker([[maybe_unused]] const std::string& marker)
    {
        return Result::kUnsupported;
    }

    void RgdClient::DumpCrash()
    {
        std::string                      path;
        const std::unique_ptr<RdfWriter> writer = client_utils_.GetRdfWriter(path);
        writer->RegisterWritingStatusEvent({.listener = this, .callback = &OnWritingStatusEvent});

        // Pass summary options to extended chunk writer for PDB support
        extended_info_chunk_.SetSummaryOptions(client_utils_.GetClientConfig().GetSummaryOptions());

        Result additional_chunk_result;
        writer->SetPostProcessOperation([&](const std::unique_ptr<ReadWriteStream>& stream) {
            std::vector<ChunkWriter*> chunk_writers = {&extended_info_chunk_};
            if (ubertrace_client_ != nullptr)
            {
                chunk_writers.push_back(ubertrace_client_.get());
            }

            additional_chunk_result = additional_chunk_writer_->Write(
                conn_info_.umd_connection_id, stream, process_info_chunk_, kAdditionalChunkDriverSettingsValues | kAdditionalChunkProcessInfo, chunk_writers);
        });

        const DD_RESULT result =
            gpu_detective_api_->TransferTraceData(gpu_detective_api_->pInstance, conn_info_.umd_connection_id, &writer->Writer(), &writer->Heartbeat());

        {
            SetState(ClientState::kDone);
        }

        if (result != DD_RESULT_SUCCESS)
        {
            client_utils_.GetLogger()->LogError("Failed to dump crash file [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));

            client_utils_.TraceCompleted(TraceCompletionStatus::kError, path, conn_info_.umd_connection_id);
            return;
        }

        client_utils_.GetLogger()->LogInfo("Successfully dumped crash file [{} {}]",
                                           conn_info_.client_pid,
                                           conn_info_.umd_connection_id,
                                           conn_info_.umd_connection_id,
                                           GetHumanReadableName(conn_info_.api));

        if (additional_chunk_result != Result::kSuccess)
        {
            client_utils_.GetLogger()->LogError("Failed to write additional chunks [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));

            client_utils_.TraceCompleted(TraceCompletionStatus::kError, path, conn_info_.umd_connection_id);
            return;
        }

        client_utils_.GetLogger()->LogInfo("Successfully wrote additional chunks [{} {}]",
                                           conn_info_.client_pid,
                                           conn_info_.umd_connection_id,
                                           conn_info_.umd_connection_id,
                                           GetHumanReadableName(conn_info_.api));

        client_utils_.TraceCompleted(TraceCompletionStatus::kCompleted, path, conn_info_.umd_connection_id);
    }
}  // namespace devtrace
