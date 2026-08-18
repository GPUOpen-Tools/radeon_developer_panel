// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Inline implmenetation for ubertrace client.

// ReSharper disable once CppMissingIncludeGuard

namespace devtrace
{

    template <typename ConfigType>
    UbertraceClient<ConfigType>::UbertraceClient(ClientConnection                              conn_info,
                                                 ClientUtils<ConfigType>&                      client_utils,
                                                 std::unique_ptr<UbertraceUser>                user,
                                                 const std::shared_ptr<AdditionalChunkWriter>& additional_chunk_writer,
                                                 const bool                                    enable_tracing)
        : conn_info_(std::move(conn_info))
        , client_utils_(client_utils)
        , user_(std::move(user))
        , additional_chunk_writer_(additional_chunk_writer)
        , enable_tracing_(enable_tracing)
    {
        user_->RegisterStateChangedEvent({this, OnUbertraceUserStateChangedEvent});
    }

    template <typename ConfigType>
    void UbertraceClient<ConfigType>::OnUbertraceUserStateChangedEvent(void* object, const UbertraceUserStateChangeEventArgs& args)
    {
        auto* self = static_cast<UbertraceClient*>(object);
        self->PostClientStateChangeEvent(ClientStateChangeEventArgs{.new_state = args.new_state, .old_state = args.old_state});
    }

    template <typename ConfigType>
    DD_DRIVER_STATE UbertraceClient<ConfigType>::GetInitDriverState()
    {
        return DD_DRIVER_STATE_PLATFORMINIT;
    }

    template <typename ConfigType>
    Result UbertraceClient<ConfigType>::Initialize()
    {
        std::vector<UberTraceSource> prelim_sources;
        GetPreliminarySources(prelim_sources);
        user_->AddPreliminarySources(prelim_sources);

        // Add early configuration (global params)
        UberTraceConfig early_config;
        GetEarlyConfiguration(client_utils_.GetClientConfig(), early_config);
        user_->AddEarlyConfiguration(early_config);

        if (const Result result = user_->Initialize(enable_tracing_); result != Result::kSuccess)
        {
            return result;
        }

        if (additional_chunk_writer_ != nullptr)
        {
            chunk_reservation_  = additional_chunk_writer_->Reserve(conn_info_.umd_connection_id);
            process_info_chunk_ = additional_chunk_writer_->PrepareProcessInfoChunk(conn_info_.client_pid);
        }

        return Result::kSuccess;
    }

    template <typename ConfigType>
    void UbertraceClient<ConfigType>::Disconnect()
    {
        timer_.Cancel();
        user_->Disconnect();

        if (additional_chunk_writer_ != nullptr)
        {
            additional_chunk_writer_->ReleaseReservation(conn_info_.umd_connection_id, chunk_reservation_);
        }
    }

    template <typename ConfigType>
    Result UbertraceClient<ConfigType>::HandleDriverState(DD_DRIVER_STATE state)
    {
        if (user_->HandleDriverState(state, client_utils_) != Result::kSuccess)
        {
            return Result::kFailure;
        }

        if (state != DD_DRIVER_STATE_POSTDEVICEINIT)
        {
            return Result::kSuccess;
        }

        if (const std::optional<UbertraceAutoCaptureConfig> auto_capture_opt = GetAutoCaptureConfig(client_utils_.GetClientConfig());
            auto_capture_opt.has_value())
        {
            return BeginAutoCapture(auto_capture_opt.value());
        }

        return Result::kSuccess;
    }

    template <typename ConfigType>
    Result UbertraceClient<ConfigType>::BeginAutoCapture(const UbertraceAutoCaptureConfig& config)
    {
        if (config.delay_ms == 0)
        {
            return RequestBeginTrace(config.capture_mode, kUberTraceCaptureTypeAutoCapture);
        }

        if (const Result prepare_result = PrepareForDelayedCapture(); prepare_result != Result::kSuccess)
        {
            return prepare_result;
        }

        // Report initial progress to show the infinite progress bar during the delay
        client_utils_.ReportCaptureProgress(WritingProgress{});

        timer_.Start(
            config.delay_ms,
            [this, config] {
                if (const Result result = RequestBeginTrace(config.capture_mode, kUberTraceCaptureTypeDelayedAutoCapture); result != Result::kSuccess)
                {
                    client_utils_.TraceCompleted(TraceCompletionStatus::kError, "", conn_info_.umd_connection_id);
                }
            },
            [this] { client_utils_.TraceCompleted(TraceCompletionStatus::kAborted, "", conn_info_.umd_connection_id); });

        return Result::kSuccess;
    }

    template <typename ConfigType>
    void UbertraceClient<ConfigType>::GetPreliminarySources([[maybe_unused]] std::vector<UberTraceSource>& sources)
    {
    }

    template <typename ConfigType>
    Result UbertraceClient<ConfigType>::RequestAbortTrace()
    {
        timer_.Cancel();
        return user_->RequestAbortTrace();
    }

    template <typename ConfigType>
    bool UbertraceClient<ConfigType>::IsAbortTraceSupported()
    {
        return user_->IsAbortTraceSupported();
    }

    template <typename ConfigType>
    ClientState UbertraceClient<ConfigType>::GetState()
    {
        return user_->GetState();
    }

    template <typename ConfigType>
    Result UbertraceClient<ConfigType>::PrepareForDelayedCapture()
    {
        return user_->PrepareForDelayedCapture();
    }

    template <typename ConfigType>
    Result UbertraceClient<ConfigType>::RequestBeginTrace(const uint32_t capture_mode)
    {
        return RequestBeginTrace(capture_mode, kUberTraceCaptureTypeNormal);
    }

    template <typename ConfigType>
    Result UbertraceClient<ConfigType>::RequestBeginTrace(const uint32_t capture_mode, const UberTraceCaptureType capture_type)
    {
        UbertraceCaptureConfig capture_config{.capture_mode = capture_mode, .capture_type = capture_type};
        if (const Result config_result = GenerateCaptureConfig(client_utils_.GetClientConfig(), capture_config); config_result != Result::kSuccess)
        {
            return config_result;
        }

        UbertraceCaptureDeps deps = {.additional_chunk_writer = additional_chunk_writer_, .process_info_chunk = process_info_chunk_};
        deps.tracing_started      = [&] { TracingStarted(); };
        deps.tracing_ended        = [&] { TracingEnded(); };
        deps.report_progress      = [&](const WritingProgress& progress) { client_utils_.ReportCaptureProgress(progress); };

        return user_->RequestBeginTrace(capture_config, client_utils_, std::move(deps));
    }

    template <typename ConfigType>
    void UbertraceClient<ConfigType>::SetIsDisabled(const bool disabled) const
    {
        user_->SetIsDisabled(disabled);
    }

    template <typename ConfigType>
    const ClientConnection& UbertraceClient<ConfigType>::GetConnInfo() const
    {
        return conn_info_;
    }

    template <typename ConfigType>
    const std::shared_ptr<Logger>& UbertraceClient<ConfigType>::GetLogger() const
    {
        return client_utils_.GetLogger();
    }

    template <typename ConfigType>
    const UbertraceFeatures& UbertraceClient<ConfigType>::GetFeatures() const
    {
        return user_->GetFeatures();
    }
}  // namespace devtrace
