// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for UberTrace trace source client.

#include "ubertrace_client.h"

#include <utility>

static constexpr uint32_t kAbortTimeoutMs = 5000;

namespace devtrace
{

    UbertraceOrchestrator::UbertraceOrchestrator(ClientConnection conn_info, DDUberTraceApi* ubertrace_api, const UbertraceFeatures& features)
        : conn_info_(std::move(conn_info))
        , ubertrace_api_(ubertrace_api)
        , features_(features)
        , disabled_users_({})
        , current_user_id_(kInvalidUbertraceUserId)
    {
    }

    void UbertraceOrchestrator::SetUserState(const UbertraceUserId user_id, const ClientState state, [[maybe_unused]] const DisabledUsers& disabled_users)
    {
        user_state_[user_id] = state;
    }

    ClientState UbertraceOrchestrator::GetUserState(const UbertraceUserId user_id)
    {
        const std::lock_guard lock(user_state_mutex_);
        if (user_state_.contains(user_id))
        {
            return user_state_.at(user_id);
        }
        return ClientState::kDisabled;
    }

    void UbertraceOrchestrator::SetState(const ClientState new_state, const UbertraceUserId relevant_user, [[maybe_unused]] const DisabledUsers& disabled_users)
    {
        const std::lock_guard lock(user_state_mutex_);

        user_state_[relevant_user] = new_state;

        const UbertraceOrchestratorStateChangedEventArgs args = {.user_id = relevant_user, .state = new_state};
        EmitStateChangedEvent(relevant_user, args);
    }

    void UbertraceOrchestrator::RegisterStateChangedEvent(const UbertraceUserId user_id, const UbertraceOrchestratorStateChangedEvent& event)
    {
        const std::lock_guard lock(state_changed_event_mutex_);
        state_changed_events_[user_id].push_back(event);
    }

    void UbertraceOrchestrator::EmitStateChangedEvent(const UbertraceUserId user_id, const UbertraceOrchestratorStateChangedEventArgs& args)
    {
        const std::lock_guard lock(state_changed_event_mutex_);
        for (auto& [listener, callback] : state_changed_events_[user_id])
        {
            if (callback)
            {
                callback(listener, args);
            }
        }
    }

    std::unique_ptr<UbertraceUser> UbertraceOrchestrator::GetNewUser()
    {
        return std::make_unique<UbertraceUser>(shared_from_this(), ++current_user_id_);
    }

    Result UbertraceOrchestrator::Initialize(const UbertraceUserId user_id, const bool enable_tracing)
    {
        const std::lock_guard lock(user_state_mutex_);

        if (const ClientState& state = user_state_[user_id]; state == ClientState::kError)
        {
            return Result::kFailure;
        }

        initialized_users_.insert(user_id);

        if (const Result init_result = initialized_users_.size() == 1 ? DoInit(user_id) : Result::kSuccess; init_result != Result::kSuccess || !enable_tracing)
        {
            if (init_result == Result::kSuccess)
            {
                SetState(ClientState::kIdle, user_id, disabled_users_);
            }
            return init_result;
        }

        return EnableTracing(user_id);
    }

    Result UbertraceOrchestrator::DoInit(const UbertraceUserId user_id)
    {
        if (ubertrace_api_->Connect(ubertrace_api_->pInstance, conn_info_.umd_connection_id) != DD_RESULT_SUCCESS)
        {
            SetState(ClientState::kError, user_id, disabled_users_);
            return Result::kFailure;
        }

        return Result::kSuccess;
    }

    Result UbertraceOrchestrator::EnableTracing(const UbertraceUserId user_id)
    {
        if (enabled_tracing_)
        {
            SetState(ClientState::kIdle, user_id, disabled_users_);
            return Result::kSuccess;
        }

        if (ubertrace_api_->EnableTracing(ubertrace_api_->pInstance, conn_info_.umd_connection_id) != DD_RESULT_SUCCESS)
        {
            SetState(ClientState::kError, user_id, disabled_users_);
            return Result::kFailure;
        }

        SetState(ClientState::kIdle, user_id, disabled_users_);

        enabled_tracing_ = true;
        return Result::kSuccess;
    }

    void UbertraceOrchestrator::Disconnect(const UbertraceUserId user_id)
    {
        bool should_join = user_id == capturing_user_;
        {
            const std::lock_guard lock(user_state_mutex_);
            should_join |= initialized_users_.size() == 1 && initialized_users_.count(user_id) == 1;
        }

        if (should_join)
        {
            should_poll_for_trace_ = false;
            if (trace_polling_thread_.joinable())
            {
                trace_polling_thread_.join();
            }
        }

        initialized_users_.erase(user_id);
        if (initialized_users_.empty())
        {
            DEV_TRACE_ASSERT(should_poll_for_trace_ == false);
            DEV_TRACE_ASSERT(!trace_polling_thread_.joinable());

            ubertrace_api_->Disconnect(ubertrace_api_->pInstance, conn_info_.umd_connection_id);

            SetState(ClientState::kDone, kInvalidUbertraceUserId, disabled_users_);
        }
    }

    void UbertraceOrchestrator::AddPreliminarySources(std::vector<UberTraceSource>& sources)
    {
        const std::lock_guard lock(user_state_mutex_);
        preliminary_sources_.reserve(preliminary_sources_.size() + sources.size());
        std::ranges::move(sources, std::back_inserter(preliminary_sources_));
    }

    void UbertraceOrchestrator::AddEarlyConfiguration(UberTraceConfig& early_config)
    {
        // Merge global_params from early_config
        if (early_config.global_params != nullptr)
        {
            preliminary_global_params_ = std::move(early_config.global_params);
        }
    }

    Result UbertraceOrchestrator::HandleDriverState(const UbertraceUserId user_id, const DD_DRIVER_STATE state, BaseClientUtils& client_utils)
    {
        const std::lock_guard lock(user_state_mutex_);
        if (state <= latest_driver_states_[user_id])
        {
            return Result::kSuccess;
        }

        latest_driver_states_[user_id] = state;
        if (state != DD_DRIVER_STATE_POSTDEVICEINIT)
        {
            return Result::kSuccess;
        }

        client_utils.GetLogger()->LogInfo("Client reached POSTDEVICEINIT state [{} {}]",
                                          conn_info_.client_pid,
                                          conn_info_.umd_connection_id,
                                          conn_info_.umd_connection_id,
                                          GetHumanReadableName(conn_info_.api));

        UberTraceConfig prelim_config{};
        prelim_config.sources       = std::move(preliminary_sources_);
        prelim_config.global_params = std::move(preliminary_global_params_);

        return UpdateConfiguration(prelim_config, client_utils);
    }

    Result UbertraceOrchestrator::RequestAbortTrace(const UbertraceUserId user_id)
    {
        if (!IsAbortTraceSupported(user_id))
        {
            return Result::kUnsupported;
        }

        if (const ClientState state = GetUserState(user_id); state == ClientState::kWaitingToBeginCapture)
        {
            SetState(ClientState::kIdle, user_id, disabled_users_);

            return Result::kSuccess;
        }

        should_poll_for_trace_ = false;
        return Result::kSuccess;
    }

    bool UbertraceOrchestrator::IsAbortTraceSupported(const UbertraceUserId user_id) const
    {
        if (const ClientState& state = user_state_.at(user_id); !IsCapturing(state))
        {
            return true;
        }

        return features_.IsCancelTraceSupported();
    }

    Result UbertraceOrchestrator::PrepareForDelayedCapture(const UbertraceUserId user_id)
    {
        const std::lock_guard lock(user_state_mutex_);

        if (const ClientState& state = user_state_.at(user_id); state != ClientState::kIdle || disabled_users_.contains(user_id))
        {
            return Result::kNotReady;
        }

        SetState(ClientState::kWaitingToBeginCapture, user_id, disabled_users_);

        return Result::kSuccess;
    }

    Result UbertraceOrchestrator::RequestBeginTrace(const UbertraceUserId   user_id,
                                                    UbertraceCaptureConfig& capture_config,
                                                    BaseClientUtils&        client_utils,
                                                    UbertraceCaptureDeps    deps)
    {
        const std::lock_guard lock(user_state_mutex_);
        if (!IsReadyForCapture(user_id))
        {
            return Result::kNotReady;
        }

        if (const Result config_update = UpdateConfiguration(capture_config.ubertrace_config, client_utils); config_update != Result::kSuccess)
        {
            SetState(ClientState::kError, kInvalidUbertraceUserId, disabled_users_);

            return config_update;
        }

        if (deps.tracing_started)
        {
            deps.tracing_started();
        }

        // Emit initial indeterminate progress for all captures
        if (deps.report_progress)
        {
            WritingProgress initial_progress{};
            initial_progress.progress = 0.0F;  // Indeterminate progress bar
            deps.report_progress(initial_progress);
        }

        if (const DD_RESULT result = ubertrace_api_->RequestTrace(ubertrace_api_->pInstance, conn_info_.umd_connection_id); result != DD_RESULT_SUCCESS)
        {
            client_utils.GetLogger()->LogError("Failed to request trace [{} {}]",
                                               conn_info_.client_pid,
                                               conn_info_.umd_connection_id,
                                               conn_info_.umd_connection_id,
                                               GetHumanReadableName(conn_info_.api));

            if (const ClientState& state = user_state_.at(user_id); state == ClientState::kWaitingToBeginCapture)
            {
                SetState(ClientState::kIdle, user_id, disabled_users_);
            }

            if (deps.tracing_ended)
            {
                deps.tracing_ended();
            }

            return Result::kFailure;
        }

        client_utils.GetLogger()->LogInfo("Successfully requested trace [{} {}]",
                                          conn_info_.client_pid,
                                          conn_info_.umd_connection_id,
                                          conn_info_.umd_connection_id,
                                          GetHumanReadableName(conn_info_.api));

        if (trace_polling_thread_.joinable())
        {
            trace_polling_thread_.join();
        }

        should_poll_for_trace_ = true;

        SetState(ClientState::kCapturing, user_id, disabled_users_);

        capturing_user_ = user_id;

        const std::reference_wrapper client_util_ref = client_utils;
        const bool                   is_auto_capture = capture_config.capture_type != kUberTraceCaptureTypeNormal;

        trace_polling_thread_ = std::thread([this, client_util_ref, on_completed = capture_config.on_completed, is_auto_capture, deps = std::move(deps)] {
            PollForTraceCollectionFinished(client_util_ref, on_completed, is_auto_capture, deps);
        });

        return Result::kSuccess;
    }

    bool UbertraceOrchestrator::IsReadyForCapture(const UbertraceUserId user_id) const
    {
        const ClientState& state = user_state_.at(user_id);
        return state == ClientState::kIdle || (state == ClientState::kWaitingToBeginCapture && !disabled_users_.contains(user_id));
    }

    Result UbertraceOrchestrator::UpdateConfiguration(UberTraceConfig& ubertrace_config, BaseClientUtils& client_utils)
    {
        auto serialized = UberTraceConfigSerializer::Serialize(ubertrace_config);
        if (!serialized.has_value())
        {
            client_utils.GetLogger()->LogError("Failed to serialize config [{} {}]: {}",
                                               conn_info_.client_pid,
                                               conn_info_.umd_connection_id,
                                               conn_info_.umd_connection_id,
                                               GetHumanReadableName(conn_info_.api),
                                               serialized.error());

            return Result::kFailure;
        }

        const std::string& json_str = serialized.value();
        const DD_RESULT    result =
            ubertrace_api_->ConfigureTraceParams(ubertrace_api_->pInstance, conn_info_.umd_connection_id, json_str.c_str(), json_str.length() + 1);

        if (result != DD_RESULT_SUCCESS)
        {
            client_utils.GetLogger()->LogError("Failed to update config [{} {}]",
                                               conn_info_.client_pid,
                                               conn_info_.umd_connection_id,
                                               conn_info_.umd_connection_id,
                                               GetHumanReadableName(conn_info_.api));

            return Result::kFailure;
        }

        client_utils.GetLogger()->LogInfo("Successfully updated config [{} {}]",
                                          conn_info_.client_pid,
                                          conn_info_.umd_connection_id,
                                          conn_info_.umd_connection_id,
                                          GetHumanReadableName(conn_info_.api));

        return Result::kSuccess;
    }

    void UbertraceOrchestrator::PollForTraceCollectionFinished(BaseClientUtils&                               client_utils,
                                                               const std::function<void(const std::string&)>& on_completed,
                                                               const bool                                     is_auto_capture,
                                                               const UbertraceCaptureDeps&                    deps)
    {
        auto        additional_chunk_result = Result::kSuccess;
        bool        collect_succeeded       = false;
        std::string path;

        while (should_poll_for_trace_)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(kCollectTracePollMs));
            bool wrote_file = false;

            const DD_RESULT result = CollectTrace(client_utils, deps, path, additional_chunk_result, wrote_file);
            if (result == features_.GetTraceReadyCode())
            {
                continue;
            }

            if (result == DD_RESULT_SUCCESS)
            {
                client_utils.GetLogger()->LogInfo("Successfully collected and dumped trace [{} {}]",
                                                  conn_info_.client_pid,
                                                  conn_info_.umd_connection_id,
                                                  conn_info_.umd_connection_id,
                                                  GetHumanReadableName(conn_info_.api));

                should_poll_for_trace_ = false;
                collect_succeeded      = true;
                break;
            }

            client_utils.GetLogger()->LogError("There was an error trying to collect a trace [{} {}]",
                                               conn_info_.client_pid,
                                               conn_info_.umd_connection_id,
                                               conn_info_.umd_connection_id,
                                               GetHumanReadableName(conn_info_.api));

            // If writing started, we should notify that a trace failed to complete.
            if (wrote_file)
            {
                client_utils.TraceCompleted(TraceCompletionStatus::kError, path, conn_info_.umd_connection_id);
            }
        }

        {
            const UbertraceUserId captured_user = capturing_user_.exchange(kInvalidUbertraceUserId);
            const std::lock_guard lock(user_state_mutex_);
            if (is_auto_capture)
            {
                SetUserIsDisabled(captured_user, true, ClientState::kDone);
            }

            if (collect_succeeded)
            {
                HandleSuccessfulTrace(captured_user, client_utils, on_completed, is_auto_capture, path, additional_chunk_result);
            }
            else
            {
                HandleUnsuccessfulTrace(captured_user, client_utils, is_auto_capture);
            }
        }

        if (deps.tracing_ended)
        {
            deps.tracing_ended();
        }
    }

    void UbertraceOrchestrator::HandleSuccessfulTrace(const UbertraceUserId                          user_id,
                                                      BaseClientUtils&                               client_utils,
                                                      const std::function<void(const std::string&)>& on_completed,
                                                      const bool                                     is_auto_capture,
                                                      const std::string&                             path,
                                                      const Result                                   additional_chunk_result)
    {
        if (additional_chunk_result != Result::kSuccess)
        {
            client_utils.GetLogger()->LogError("Failed to write additional chunks [{} {}]",
                                               conn_info_.client_pid,
                                               conn_info_.umd_connection_id,
                                               conn_info_.umd_connection_id,
                                               GetHumanReadableName(conn_info_.api));

            client_utils.TraceCompleted(TraceCompletionStatus::kError, path, conn_info_.umd_connection_id);

            SetState(ClientState::kIdle, user_id, disabled_users_);

            return;
        }

        client_utils.GetLogger()->LogInfo("Successfully wrote additional chunks [{} {}]",
                                          conn_info_.client_pid,
                                          conn_info_.umd_connection_id,
                                          conn_info_.umd_connection_id,
                                          GetHumanReadableName(conn_info_.api));

        if (on_completed)
        {
            SetState(ClientState::kPostProcessing, user_id, disabled_users_);
            on_completed(path);
        }
        else
        {
            client_utils.TraceCompleted(TraceCompletionStatus::kCompleted, path, conn_info_.umd_connection_id);
        }

        // For auto-capture, stay in kDone state. For manual capture, return to kIdle.
        const ClientState final_state = is_auto_capture ? ClientState::kDone : ClientState::kIdle;
        SetState(final_state, user_id, disabled_users_);
    }

    void UbertraceOrchestrator::HandleUnsuccessfulTrace(const UbertraceUserId user_id, BaseClientUtils& client_utils, const bool is_auto_capture)
    {
        const bool aborted = AbortTrace();
        if (!aborted)
        {
            client_utils.GetLogger()->LogError("Failed to abort trace [{} {}]",
                                               conn_info_.client_pid,
                                               conn_info_.umd_connection_id,
                                               conn_info_.umd_connection_id,
                                               GetHumanReadableName(conn_info_.api));

            client_utils.TraceCompleted(TraceCompletionStatus::kError, "", conn_info_.umd_connection_id);

            SetState(ClientState::kError, user_id, disabled_users_);
            return;
        }

        client_utils.GetLogger()->LogWarning("Trace was aborted [{} {}]",
                                             conn_info_.client_pid,
                                             conn_info_.umd_connection_id,
                                             conn_info_.umd_connection_id,
                                             GetHumanReadableName(conn_info_.api));

        client_utils.TraceCompleted(TraceCompletionStatus::kAborted, "", conn_info_.umd_connection_id);

        // For auto-capture, stay in kDone state since they are one-shot. For manual capture, return to kIdle.
        const ClientState final_state = is_auto_capture ? ClientState::kDone : ClientState::kIdle;
        SetState(final_state, user_id, disabled_users_);
    }

    bool UbertraceOrchestrator::AbortTrace()
    {
        SetState(ClientState::kBusy, kInvalidUbertraceUserId, disabled_users_);

        const auto start = std::chrono::system_clock::now();
        while (std::chrono::system_clock::now() - start < std::chrono::milliseconds(kAbortTimeoutMs))
        {
            switch (ubertrace_api_->CancelTrace(ubertrace_api_->pInstance, conn_info_.umd_connection_id))
            {
            case DD_RESULT_SUCCESS:
                return true;
            case DD_RESULT_DD_GENERIC_NOT_READY:
                break;
            default:
                return false;
            }
        }

        return false;
    }

    void UbertraceOrchestrator::SetUserIsDisabled(const UbertraceUserId user_id, const bool disabled)
    {
        const std::lock_guard lock(user_state_mutex_);
        SetUserIsDisabled(user_id, disabled, ClientState::kDisabled);
    }

    void UbertraceOrchestrator::SetUserIsDisabled(const UbertraceUserId user_id, const bool disabled, const ClientState state)
    {
        const std::lock_guard lock(disabled_user_mutex_);

        if (disabled)
        {
            disabled_users_.insert({user_id, state});
        }
        else if (disabled_users_.contains(user_id) && disabled_users_[user_id] == state)
        {
            disabled_users_.erase(user_id);
        }

        // Emit state changed event to broadcast change in disabled users.
        SetState(state, user_id, disabled_users_);
    }

    const UbertraceFeatures& UbertraceOrchestrator::GetFeatures() const
    {
        return features_;
    }

    void UbertraceOrchestrator::OnWritingStatusEvent(void* object, const WritingStatusEventArgs& args)
    {
        auto* self = static_cast<UbertraceOrchestrator*>(object);
        if (args.is_writing)
        {
            self->SetState(ClientState::kDumping, self->capturing_user_, self->disabled_users_);
        }
    }

    DD_RESULT UbertraceOrchestrator::CollectTrace(BaseClientUtils&            client_utils,
                                                  const UbertraceCaptureDeps& deps,
                                                  std::string&                path,
                                                  Result&                     additional_chunk_result,
                                                  bool&                       wrote_file)
    {
        const std::unique_ptr<ByteWriter> writer = client_utils.GetByteWriter(path);
        writer->RegisterWritingStatusEvent({.listener = this, .callback = &OnWritingStatusEvent});
        writer->SetPostProcessOperation([&](const std::unique_ptr<ReadWriteStream>& stream) {
            if (deps.additional_chunk_writer != nullptr)
            {
                additional_chunk_result = deps.additional_chunk_writer->Write(conn_info_.umd_connection_id, stream, deps.process_info_chunk);
            }
        });

        const DD_RESULT result = ubertrace_api_->CollectTrace(ubertrace_api_->pInstance, conn_info_.umd_connection_id, kDumpTimeoutMs, &writer->Writer());
        wrote_file             = false;

        return result;
    }

    void UbertraceUser::OnOrchestratorStateChangedEvent(void* listener, const UbertraceOrchestratorStateChangedEventArgs& args)
    {
        auto* self = static_cast<UbertraceUser*>(listener);

        const ClientState old_state = self->state_;
        self->state_                = args.state;

        // Broadcast ubertrace user state change to trace source
        self->EmitStateChangedEvent({.new_state = self->state_, .old_state = old_state});
    }

    UbertraceUser::UbertraceUser(const std::shared_ptr<UbertraceOrchestrator>& orchestrator, const UbertraceUserId user_id)
        : orchestrator_(orchestrator)
        , user_id_(user_id)
        , state_(ClientState::kIdle)
    {
        orchestrator_->RegisterStateChangedEvent(user_id, {.listener = this, .callback = OnOrchestratorStateChangedEvent});
    }

    Result UbertraceUser::Initialize(const bool enable_tracing) const
    {
        return orchestrator_->Initialize(user_id_, enable_tracing);
    }

    void UbertraceUser::Disconnect() const
    {
        orchestrator_->Disconnect(user_id_);
    }

    void UbertraceUser::AddPreliminarySources(std::vector<UberTraceSource>& sources) const
    {
        orchestrator_->AddPreliminarySources(sources);
    }

    void UbertraceUser::AddEarlyConfiguration(UberTraceConfig& early_config) const
    {
        orchestrator_->AddEarlyConfiguration(early_config);
    }

    Result UbertraceUser::HandleDriverState(const DD_DRIVER_STATE state, BaseClientUtils& client_utils) const
    {
        return orchestrator_->HandleDriverState(user_id_, state, client_utils);
    }

    Result UbertraceUser::RequestAbortTrace() const
    {
        return orchestrator_->RequestAbortTrace(user_id_);
    }

    bool UbertraceUser::IsAbortTraceSupported() const
    {
        return orchestrator_->IsAbortTraceSupported(user_id_);
    }

    ClientState UbertraceUser::GetState() const
    {
        return state_;
    }

    Result UbertraceUser::PrepareForDelayedCapture() const
    {
        return orchestrator_->PrepareForDelayedCapture(user_id_);
    }

    Result UbertraceUser::RequestBeginTrace(UbertraceCaptureConfig& capture_config, BaseClientUtils& client_utils, UbertraceCaptureDeps deps) const
    {
        return orchestrator_->RequestBeginTrace(user_id_, capture_config, client_utils, std::move(deps));
    }

    void UbertraceUser::SetIsDisabled(const bool disabled) const
    {
        orchestrator_->SetUserIsDisabled(user_id_, disabled);
    }

    const UbertraceFeatures& UbertraceUser::GetFeatures() const
    {
        return orchestrator_->GetFeatures();
    }

    void UbertraceUser::RegisterStateChangedEvent(const UbertraceUserStateChangeEvent& event)
    {
        const std::lock_guard lock(state_changed_event_mutex_);
        state_changed_event_ = event;
    }

    void UbertraceUser::EmitStateChangedEvent(const UbertraceUserStateChangeEventArgs& args)
    {
        const std::lock_guard lock(state_changed_event_mutex_);
        if (state_changed_event_.callback)
        {
            state_changed_event_.callback(state_changed_event_.listener, args);
        }
    }
}  // namespace devtrace
