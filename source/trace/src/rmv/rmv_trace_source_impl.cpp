// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the concrete RMV trace source.

#include "rmv_trace_source_impl.h"

#include <string>
#include <utility>

#include <ddApi.h>
#include <dd_memory_trace_api.h>

#include "dev_trace_common.h"
#include "system_info_cache.h"

namespace devtrace
{
    void RmvClient::OnWritingStatusEvent(void* object, const WritingStatusEventArgs& args)
    {
        auto* self = static_cast<RmvClient*>(object);
        if (args.is_writing)
        {
            self->SetState(ClientState::kDumping);
        }
    }

    RmvClient::RmvClient(ClientConnection                              conn_info,
                         ClientUtils<RmvTraceSourceConfig>&            client_utils,
                         DDMemoryTraceApi*                             memory_trace_api,
                         const std::shared_ptr<AdditionalChunkWriter>& additional_chunk_writer,
                         bool                                          enable_etw)
        : conn_info_(std::move(conn_info))
        , memory_trace_api_(memory_trace_api)
        , additional_chunk_writer_(additional_chunk_writer)
        , client_utils_(client_utils)
        , enable_etw_(enable_etw)
        , process_info_chunk_()
    {
    }

    RmvClient::~RmvClient() = default;

    DD_DRIVER_STATE RmvClient::GetInitDriverState()
    {
        return DD_DRIVER_STATE_PLATFORMINIT;
    }

    Result RmvClient::Initialize()
    {
        if (const DD_RESULT result =
                memory_trace_api_->EnableTracing(memory_trace_api_->pInstance, conn_info_.umd_connection_id, conn_info_.client_pid, enable_etw_);
            result != DD_RESULT_SUCCESS)
        {
            SetState(GetMemoryTracingClientState());
            client_utils_.GetLogger()->LogError("Failed to initialize memory tracing [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));

            return Result::kFailure;
        }

        SetState(GetMemoryTracingClientState());
        client_utils_.GetLogger()->LogInfo("Successfully initialized memory tracing with ETW {} [{} {}]",
                                           conn_info_.client_pid,
                                           conn_info_.umd_connection_id,
                                           enable_etw_ ? "enabled" : "disabled",
                                           conn_info_.umd_connection_id,
                                           GetHumanReadableName(conn_info_.api));

        chunk_reservation_  = additional_chunk_writer_->Reserve(conn_info_.umd_connection_id);
        process_info_chunk_ = additional_chunk_writer_->PrepareProcessInfoChunk(conn_info_.client_pid);

        return Result::kSuccess;
    }

    void RmvClient::Disconnect()
    {
        if (GetMemoryTracingClientState() == ClientState::kCapturing)
        {
            if (has_reached_post_device_init_ && state_ != ClientState::kError)
            {
                if (const auto result = RequestDump(); result != Result::kSuccess)
                {
                    client_utils_.GetLogger()->LogError("Failed to dump trace on disconnect [{} {}]",
                                                        conn_info_.client_pid,
                                                        conn_info_.umd_connection_id,
                                                        conn_info_.umd_connection_id,
                                                        GetHumanReadableName(conn_info_.api));
                }
            }
        }

        if (!has_reached_post_device_init_)
        {
            // Clients that have not reached post device init will not have their traces ended through normal means so we need to
            // ensure we end tracing here to join any tracing threads.
            std::ignore = EndTracing();
        }

        memory_trace_api_->DisableTracing(memory_trace_api_->pInstance, conn_info_.umd_connection_id);
        additional_chunk_writer_->ReleaseReservation(conn_info_.umd_connection_id, chunk_reservation_);
    }

    Result RmvClient::RequestAbortTrace()
    {
        if (const Result result = EndTracing(); result == Result::kSuccess)
        {
            client_utils_.GetLogger()->LogInfo("Trace was aborted [{} {}]",
                                               conn_info_.client_pid,
                                               conn_info_.umd_connection_id,
                                               conn_info_.umd_connection_id,
                                               GetHumanReadableName(conn_info_.api));

            client_utils_.TraceCompleted(TraceCompletionStatus::kAborted, "", conn_info_.umd_connection_id);
            return Result::kSuccess;
        }

        client_utils_.GetLogger()->LogError("Failed to abort trace [{} {}]",
                                            conn_info_.client_pid,
                                            conn_info_.umd_connection_id,
                                            conn_info_.umd_connection_id,
                                            GetHumanReadableName(conn_info_.api));

        return Result::kFailure;
    }

    bool RmvClient::IsAbortTraceSupported()
    {
        return true;
    }

    Result RmvClient::HandleDriverState(const DD_DRIVER_STATE state)
    {
        if (state == DD_DRIVER_STATE_POSTDEVICEINIT)
        {
            client_utils_.GetLogger()->LogInfo("Client reached POSTDEVICEINIT state [{} {}]",
                                               conn_info_.client_pid,
                                               conn_info_.umd_connection_id,
                                               conn_info_.umd_connection_id,
                                               GetHumanReadableName(conn_info_.api));
            has_reached_post_device_init_ = true;
        }

        return Result::kSuccess;
    }

    Result RmvClient::RequestDump()
    {
        if (state_ != ClientState::kCapturing)
        {
            return Result::kFailure;
        }

        if (const Result result = EndTracing(); result != Result::kSuccess)
        {
            return result;
        }

        return Dump();
    }

    Result RmvClient::AddMarker(const std::string& marker)
    {
        const std::scoped_lock lock(state_mutex_);

        if (state_ != ClientState::kCapturing)
        {
            return Result::kFailure;
        }

        if (const DD_RESULT result = memory_trace_api_->InsertSnapshot(memory_trace_api_->pInstance, conn_info_.umd_connection_id, marker.c_str());
            result != DD_RESULT_SUCCESS)
        {
            client_utils_.GetLogger()->LogError("Failed to add snapshot [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));

            return Result::kFailure;
        }

        client_utils_.GetLogger()->LogInfo("Added snapshot \"{}\" [{} {}]",
                                           conn_info_.client_pid,
                                           conn_info_.umd_connection_id,
                                           marker,
                                           conn_info_.umd_connection_id,
                                           GetHumanReadableName(conn_info_.api));

        return Result::kSuccess;
    }

    Result RmvClient::EndTracing()
    {
        const DD_RESULT result = memory_trace_api_->EndTracing(memory_trace_api_->pInstance, conn_info_.umd_connection_id, has_reached_post_device_init_);

        const ClientState new_state = result == DD_RESULT_SUCCESS ? GetMemoryTracingClientState() : ClientState::kError;
        SetState(new_state);

        if (result != DD_RESULT_SUCCESS)
        {
            client_utils_.GetLogger()->LogError("Failed to end tracing [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));

            return Result::kFailure;
        }

        client_utils_.GetLogger()->LogInfo(
            "Ended tracing [{} {}]", conn_info_.client_pid, conn_info_.umd_connection_id, conn_info_.umd_connection_id, GetHumanReadableName(conn_info_.api));

        return Result::kSuccess;
    }

    Result RmvClient::Dump()
    {
        if (GetState() != ClientState::kDone)
        {
            return Result::kFailure;
        }

        std::string                      path;
        const std::unique_ptr<RdfWriter> writer = client_utils_.GetRdfWriter(path);
        writer->RegisterWritingStatusEvent({.listener = this, .callback = &OnWritingStatusEvent});

        Result additional_chunk_result;
        writer->SetPostProcessOperation([&](const std::unique_ptr<ReadWriteStream>& stream) {
            additional_chunk_result = additional_chunk_writer_->Write(
                conn_info_.umd_connection_id, stream, process_info_chunk_, kAdditionalChunkDriverSettingsValues | kAdditionalChunkProcessInfo);
        });

        const DD_RESULT result =
            memory_trace_api_->TransferTraceData(memory_trace_api_->pInstance, conn_info_.umd_connection_id, &writer->Writer(), &writer->Heartbeat(), true);

        if (result != DD_RESULT_SUCCESS)
        {
            client_utils_.GetLogger()->LogError("Failed to dump trace [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));

            client_utils_.TraceCompleted(TraceCompletionStatus::kError, path, conn_info_.umd_connection_id);

            SetState(ClientState::kDone);

            return Result::kFailure;
        }

        client_utils_.GetLogger()->LogInfo("Successfully dumped trace [{} {}]",
                                           conn_info_.client_pid,
                                           conn_info_.umd_connection_id,
                                           conn_info_.umd_connection_id,
                                           GetHumanReadableName(conn_info_.api));

        memory_trace_api_->ClearTrace(memory_trace_api_->pInstance, conn_info_.umd_connection_id);

        if (additional_chunk_result != Result::kSuccess)
        {
            client_utils_.GetLogger()->LogError("Failed to write additional chunks [{} {}]",
                                                conn_info_.client_pid,
                                                conn_info_.umd_connection_id,
                                                conn_info_.umd_connection_id,
                                                GetHumanReadableName(conn_info_.api));

            client_utils_.TraceCompleted(TraceCompletionStatus::kError, path, conn_info_.umd_connection_id);

            SetState(ClientState::kDone);

            return Result::kFailure;
        }

        client_utils_.GetLogger()->LogInfo("Successfully wrote additional chunks [{} {}]",
                                           conn_info_.client_pid,
                                           conn_info_.umd_connection_id,
                                           conn_info_.umd_connection_id,
                                           GetHumanReadableName(conn_info_.api));

        client_utils_.TraceCompleted(TraceCompletionStatus::kCompleted, path, conn_info_.umd_connection_id);

        SetState(ClientState::kDone);

        return Result::kSuccess;
    }

    ClientState RmvClient::GetMemoryTracingClientState() const
    {
        DDMemoryTraceStatus trace_status{};
        if (memory_trace_api_->QueryStatus(memory_trace_api_->pInstance, conn_info_.umd_connection_id, &trace_status) != DD_RESULT_SUCCESS)
        {
            return ClientState::kError;
        }

        switch (trace_status.state)
        {
        case DD_MEMORY_TRACE_STATE_RUNNING:
            return ClientState::kCapturing;
        case DD_MEMORY_TRACE_STATE_UNKNOWN:
            return ClientState::kError;
        default:
            return ClientState::kDone;
        }
    }

    RmvClientFactory::RmvClientFactory(const std::shared_ptr<SystemInfoCache>&       system_info_cache,
                                       DDMemoryTraceApi*                             memory_trace_api,
                                       const std::shared_ptr<AdditionalChunkWriter>& additional_chunk_writer)
        : system_info_cache_(system_info_cache)
        , memory_trace_api_(memory_trace_api)
        , addl_chunk_writer_(additional_chunk_writer)
    {
    }

    RmvClientFactory::~RmvClientFactory() = default;

    std::unique_ptr<RmvClient> RmvClientFactory::CreateClient(const ClientConnection& info, ClientUtils<RmvTraceSourceConfig>& client_utils)
    {
        // Determine if ETW should be enabled based on system info
        bool enable_etw = false;
        if (system_info_cache_ != nullptr)
        {
            if (const auto system_info = system_info_cache_->GetSystemInfo(); system_info.has_value())
            {
                // Enable ETW if it's supported AND has permissions
                enable_etw = system_info->os.config.etw_support_info.is_supported && system_info->os.config.etw_support_info.has_permission;
            }
        }

        return std::make_unique<RmvClient>(info, client_utils, memory_trace_api_, addl_chunk_writer_, enable_etw);
    }

    RmvTraceSourceImpl::RmvTraceSourceImpl(const std::shared_ptr<ReadWriteStreamProvider>& stream_provider,
                                           const std::shared_ptr<OverlayManager>&          overlay_manager,
                                           const std::shared_ptr<SystemInfoCache>&         system_info_cache,
                                           const std::shared_ptr<AdditionalChunkWriter>&   additional_chunk_writer,
                                           DDMemoryTraceApi*                               memory_trace_api,
                                           const std::shared_ptr<Logger>&                  logger)
        : logger_(logger->WithSource("RMV Trace Source"))
        , base_trace_source_(std::make_shared<RmvClientFactory>(system_info_cache, memory_trace_api, additional_chunk_writer),
                             stream_provider,
                             overlay_manager,
                             OverlayFeature::kRmv,
                             logger_)
    {
    }

    void RmvTraceSourceImpl::OnDriverConnected(const DDConnectionInfo& connection_info)
    {
        base_trace_source_.OnDriverConnected(connection_info);
    }

    void RmvTraceSourceImpl::OnDriverDisconnected(const DDConnectionId umd_connection_id)
    {
        base_trace_source_.OnDriverDisconnected(umd_connection_id);
    }

    void RmvTraceSourceImpl::OnDriverStateChanged(const DDConnectionId umd_connection_id, const DD_DRIVER_STATE state)
    {
        base_trace_source_.OnDriverStateChanged(umd_connection_id, state);
    }

    void RmvTraceSourceImpl::RegisterStatusEvent(const TraceSourceStatusEvent& event)
    {
        base_trace_source_.RegisterStatusEvent(event);
    }

    void RmvTraceSourceImpl::RegisterTraceCompletionEvent(const TraceCompletionEvent& event)
    {
        base_trace_source_.RegisterTraceCompletionEvent(event);
    }

    void RmvTraceSourceImpl::RegisterTraceCaptureProgressEvent(const TraceCaptureProgressEvent& event)
    {
        base_trace_source_.RegisterTraceCaptureProgressEvent(event);
    }

    void RmvTraceSourceImpl::QueryStatus()
    {
        base_trace_source_.QueryStatus();
    }

    Result RmvTraceSourceImpl::RequestAbortTrace(DDConnectionId umd_connection_id)
    {
        return base_trace_source_.RequestAbortTrace(umd_connection_id);
    }

    Result RmvTraceSourceImpl::RequestAbortProcessing()
    {
        return Result::kSuccess;
    }

    RmvTraceSourceConfig& RmvTraceSourceImpl::GetConfig()
    {
        return base_trace_source_.GetConfig();
    }

    Result RmvTraceSourceImpl::AddMarker(const DDConnectionId connection_id, const std::string& marker)
    {
        return base_trace_source_.AddMarker(connection_id, marker);
    }

    Result RmvTraceSourceImpl::RequestDump(const DDConnectionId connection_id)
    {
        return base_trace_source_.RequestDump(connection_id);
    }

}  // namespace devtrace
