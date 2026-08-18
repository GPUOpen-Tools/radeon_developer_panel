// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for base trace source.

// ReSharper disable once CppMissingIncludeGuard

namespace devtrace
{
    template <typename ClientType>
    void BaseTraceSource<ClientType>::TraceCompletionThreadFunc()
    {
#ifdef _WIN32
        SetCurrentThreadName(L"TraceCompletionReporting");
#else
        SetCurrentThreadName("TraceCompletionReporting");
#endif

        while (true)
        {
            std::unique_lock lock(trace_completion_queue_mutex_);
            trace_completion_queue_cv_.wait(lock, [this] { return trace_completion_thread_exit_ || !trace_completion_event_queue_.empty(); });

            if (trace_completion_thread_exit_ && trace_completion_event_queue_.empty())
            {
                break;
            }

            auto completion_args = trace_completion_event_queue_.front();
            trace_completion_event_queue_.pop();
            lock.unlock();

            std::scoped_lock events_lock(trace_completion_events_mutex_);
            for (auto& [listener, callback] : trace_completion_events_)
            {
                if (callback)
                {
                    callback(listener, completion_args);
                }
            }
        }
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::CaptureProgressThreadFunc()
    {
#ifdef _WIN32
        SetCurrentThreadName(L"TraceCaptureProgressReporting");
#else
        SetCurrentThreadName("TraceCaptureProgressReporting");
#endif

        while (true)
        {
            std::unique_lock lock(capture_progress_queue_mutex_);
            capture_progress_queue_cv_.wait(lock, [this] { return capture_progress_thread_exit_ || !capture_progress_event_queue_.empty(); });

            if (capture_progress_thread_exit_ && capture_progress_event_queue_.empty())
            {
                break;
            }

            auto event_args = capture_progress_event_queue_.front();
            capture_progress_event_queue_.pop();
            lock.unlock();

            std::scoped_lock events_lock(capture_progress_events_mutex_);
            for (auto& [listener, callback] : capture_progress_events_)
            {
                if (callback)
                {
                    callback(listener, event_args);
                }
            }
        }
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::TraceStatusThreadFunc()
    {
#ifdef _WIN32
        SetCurrentThreadName(L"TraceStatusReporting");
#else
        SetCurrentThreadName("TraceStatusReporting");
#endif

        while (true)
        {
            std::unique_lock lock(status_queue_mutex_);
            status_queue_cv_.wait(lock, [this] { return status_thread_exit_ || !status_event_queue_.empty(); });

            if (status_thread_exit_ && status_event_queue_.empty())
            {
                break;
            }

            auto event_args = status_event_queue_.front();
            status_event_queue_.pop();
            lock.unlock();

            std::scoped_lock events_lock(status_events_mutex_);
            for (auto& [listener, callback] : status_events_)
            {
                if (callback)
                {
                    callback(listener, event_args);
                }
            }
        }
    }

    template <typename ClientType>
    BaseTraceSource<ClientType>::BaseTraceSource(const std::shared_ptr<ClientFactoryType>&       client_factory,
                                                 const std::shared_ptr<ReadWriteStreamProvider>& stream_provider,
                                                 const std::shared_ptr<OverlayManager>&          overlay_manager,
                                                 const OverlayFeature                            overlay_feature,
                                                 const std::shared_ptr<Logger>&                  logger,
                                                 IClientEventBindingDelegate<ClientType>*        binding_delegate)

        : client_factory_(client_factory)
        , stream_provider_(stream_provider)
        , overlay_manager_(overlay_manager)
        , overlay_feature_(overlay_feature)
        , logger_(logger)
        , binding_delegate_(binding_delegate)
        , supported_apis_({Api::kVulkan, Api::kDirectX12})
        , system_disabled_reason_(DisabledReason::kEnabled)
    {
        // This thread logic handles posting TraceStatus events to registered listeners.
        status_thread_ = std::thread(&BaseTraceSource::TraceStatusThreadFunc, this);

        // This thread logic handles posting TraceCaptureProgress events to registered listeners.
        capture_progress_thread_ = std::thread(&BaseTraceSource::CaptureProgressThreadFunc, this);

        // This thread logic handles posting TraceCompletion events to registered listeners.
        trace_completion_thread_ = std::thread(&BaseTraceSource::TraceCompletionThreadFunc, this);
    }

    template <typename ClientType>
    BaseTraceSource<ClientType>::~BaseTraceSource()
    {
        // Signal all threads to exit and wake them up
        {
            std::scoped_lock lock(status_queue_mutex_);
            status_thread_exit_ = true;
        }
        status_queue_cv_.notify_all();
        if (status_thread_.joinable())
        {
            status_thread_.join();
        }

        {
            std::scoped_lock lock(trace_completion_queue_mutex_);
            trace_completion_thread_exit_ = true;
        }
        trace_completion_queue_cv_.notify_all();
        if (trace_completion_thread_.joinable())
        {
            trace_completion_thread_.join();
        }

        {
            std::scoped_lock lock(capture_progress_queue_mutex_);
            capture_progress_thread_exit_ = true;
        }
        capture_progress_queue_cv_.notify_all();
        if (capture_progress_thread_.joinable())
        {
            capture_progress_thread_.join();
        }
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::OnClientStateChangeEvent(void* object, [[maybe_unused]] const ClientStateChangeEventArgs& args)
    {
        auto* self = static_cast<BaseTraceSource*>(object);

        DDConnectionId umd_connection_id{};
        {
            std::unique_lock client_lock(self->client_mutex_);
            if (self->clients_.empty())  // Ideally, should never happen, see below
            {
                return;
            }

            // TODO: This callback should pass a umd connection id to be more specific about which client changed state,
            // since we do technically support multiple UMD connections.
            umd_connection_id = self->clients_.front()->conn_info.umd_connection_id;
        }

        self->overlay_manager_->UpdateFeatureState(umd_connection_id, self->overlay_feature_, ClientStateToOverlayState(args.new_state));
        self->UpdateTraceSourceStatus();
    }

    template <typename ClientType>
    BaseTraceSource<ClientType>::ClientConfigType& BaseTraceSource<ClientType>::GetConfig()
    {
        return config_;
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::OnDriverConnected(const DDConnectionInfo& connection_info)
    {
        const Api        connection_api = GetApiFromDriverDescription(connection_info.pDescription);
        ClientConnection connection     = {.api               = connection_api,
                                           .umd_connection_id = connection_info.umdConnectionId,
                                           .client_pid        = connection_info.processId,
                                           .client_name       = connection_info.pProcessName};

        if (system_disabled_reason_ != DisabledReason::kEnabled)
        {
            overlay_manager_->UpdateFeatureState(connection.umd_connection_id, overlay_feature_, OverlayState::kDisabled);
            logger_->LogInfo("Client ignored because system was disabled [{} {}]",
                             connection.client_pid,
                             connection.umd_connection_id,
                             connection.umd_connection_id,
                             GetHumanReadableName(connection.api));

            return;
        }

        logger_->LogInfo("Client connected [{} {}]",
                         connection.client_pid,
                         connection.umd_connection_id,
                         connection.umd_connection_id,
                         GetHumanReadableName(connection.api));

        {
            std::unique_lock client_lock(client_mutex_);

            const bool passes_api_filter = supported_apis_.contains(connection_api);

            if (passes_api_filter)
            {
                auto new_client = client_factory_->CreateClient(connection, *this);
                pending_clients_.emplace_back(std::make_unique<BoundClient>(connection, new_client));
            }
            else
            {
                overlay_manager_->UpdateFeatureState(connection.umd_connection_id, overlay_feature_, OverlayState::kDisabled);
                logger_->LogInfo("Client ignored because it did not match the API filter [{} {}]",
                                 connection.client_pid,
                                 connection.umd_connection_id,
                                 connection.umd_connection_id,
                                 GetHumanReadableName(connection.api));

                unsupported_clients_.emplace_back(connection);
            }
        }

        UpdateTraceSourceStatus();
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::OnDriverDisconnected(const DDConnectionId umd_connection_id)
    {
        std::unique_ptr<BoundClient> client_to_disconnect;

        {
            std::unique_lock client_lock(client_mutex_);

            auto bound_client = FindClient(umd_connection_id);
            if (bound_client != clients_.end())
            {
                logger_->LogInfo("Client disconnected [{} {}]",
                                 (*bound_client)->conn_info.client_pid,
                                 (*bound_client)->conn_info.umd_connection_id,
                                 (*bound_client)->conn_info.umd_connection_id,
                                 GetHumanReadableName((*bound_client)->conn_info.api));

                // Grab the client out of the client list as we will be disconnecting it.
                client_to_disconnect = std::move(*bound_client);
                clients_.erase(bound_client);
            }

            const auto unsupported_client = std::find_if(unsupported_clients_.begin(), unsupported_clients_.end(), [&](const ClientConnection& info) {
                return info.umd_connection_id == umd_connection_id;
            });

            if (unsupported_client != unsupported_clients_.end())
            {
                unsupported_clients_.erase(unsupported_client);
            }

            const auto pending_client = FindPendingClient(umd_connection_id);
            if (pending_client != pending_clients_.end())
            {
                pending_clients_.erase(pending_client);
            }
        }

        // Now if we have a client to disconnect, do it here allowing any side-effects to happen without holding the client lock.
        if (client_to_disconnect)
        {
            client_to_disconnect->client->Disconnect();

            logger_->LogInfo("Finished disconnecting client [{} {}]",
                             client_to_disconnect->conn_info.client_pid,
                             client_to_disconnect->conn_info.umd_connection_id,
                             client_to_disconnect->conn_info.umd_connection_id,
                             GetHumanReadableName(client_to_disconnect->conn_info.api));
        }

        UpdateTraceSourceStatus();
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::OnDriverStateChanged(const DDConnectionId umd_connection_id, DD_DRIVER_STATE state)
    {
        InitClientIfNeeded(umd_connection_id, state);

        auto bound_client = GetBoundClient(umd_connection_id);
        if (bound_client != nullptr)
        {
            std::ignore = bound_client->client->HandleDriverState(state);
        }
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::EmitPostProcessEvent(const std::string& progress_text, float progress)
    {
        TraceCaptureProgress new_progress;
        new_progress.num_bytes_dumped    = 0;
        new_progress.total_bytes_to_dump = 0;
        new_progress.progress            = progress;
        new_progress.progress_text       = progress_text;

        {
            std::scoped_lock client_lock(client_mutex_);
            new_progress.status = BuildCurrentStatus(TraceSourceStage::kProcessing);
        }

        TraceCaptureProgressEventArgs args;
        args.old_progress = {};
        args.new_progress = new_progress;

        {
            std::lock_guard lock(capture_progress_queue_mutex_);
            capture_progress_event_queue_.push(args);
            previous_progress_ = new_progress;
        }
        capture_progress_queue_cv_.notify_one();
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::InitClientIfNeeded(DDConnectionId umd_connection_id, DD_DRIVER_STATE state)
    {
        std::unique_ptr<BoundClient> client{};
        {
            std::unique_lock client_lock(client_mutex_);

            const auto find_result = FindPendingClient(umd_connection_id);
            if (find_result == pending_clients_.end())
            {
                return;
            }

            if (state != (*find_result)->client->GetInitDriverState())
            {
                return;
            }

            client = std::move(*find_result);

            pending_clients_.erase(find_result);
        }

        logger_->LogInfo("Client reached init state [{} {}]",
                         client->conn_info.client_pid,
                         umd_connection_id,
                         umd_connection_id,
                         GetHumanReadableName(client->conn_info.api));

        Result init_result;
        {
            std::unique_lock init_lock = serialize_connection_init_ ? std::unique_lock(init_mutex_) : std::unique_lock<std::mutex>();
            init_result                = client->client->Initialize();
        }

        if (init_result != Result::kSuccess)
        {
            logger_->LogError("Failed to initialize client for API {}",
                              client->conn_info.client_pid,
                              client->conn_info.umd_connection_id,
                              GetHumanReadableName(client->conn_info.api));
            return;
        }

        logger_->LogInfo(
            "Initialized new client [{} {}]", client->conn_info.client_pid, umd_connection_id, umd_connection_id, GetHumanReadableName(client->conn_info.api));

        {
            std::unique_lock client_lock(client_mutex_);
            clients_.emplace_back(std::move(client));

            auto bound_client = FindClient(umd_connection_id);
            if (bound_client == clients_.end())
            {
                // DevDriver currently uses one thread per client, so a client should never be able to disconnect
                // before it finishes connecting (in this thread). If we ever hit this assert, it means that either there is a mistake
                // somewhere in the base trace source or the DevDriver implementation has changed.
                logger_->LogError("CRITICAL ERROR: Client disconnected on another thread! [{}]", kLoggingInvalidPid, umd_connection_id, umd_connection_id);
                DEV_TRACE_ASSERT(false);
                return;
            }

            if (binding_delegate_)
            {
                binding_delegate_->Bind((*bound_client)->client.get());
            }

            (*bound_client)->client->RegisterClientStateChangeEvent({this, OnClientStateChangeEvent});

            if (no_crash_detected_callback_)
            {
                (*bound_client)->client->RegisterNoCrashDetectedEvent({.listener = this, .callback = [](void* listener) {
                                                                           auto* self = static_cast<BaseTraceSource*>(listener);
                                                                           if (self->no_crash_detected_callback_)
                                                                           {
                                                                               self->no_crash_detected_callback_();
                                                                           }
                                                                       }});
            }

            HandleDeveloperModeOverlay(*bound_client);
        }

        UpdateTraceSourceStatus();
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::HandleDeveloperModeOverlay(const std::unique_ptr<BoundClient>& bound_client)
    {
        const DDConnectionId umd_connection_id = bound_client->conn_info.umd_connection_id;
        overlay_manager_->UpdateFeatureState(umd_connection_id, overlay_feature_, ClientStateToOverlayState(bound_client->client->GetState()));
    }

    template <typename ClientType>
    TraceSourceStatus BaseTraceSource<ClientType>::BuildCurrentStatus()
    {
        TraceSourceStatus new_status{};
        new_status.ChangeStage(GetTraceSourceStage());

        FillProcessInfo(new_status);
        FillDisabledReason(new_status);
        FillCurrentConnections(new_status);
        FillAbortSupported(new_status);

        return new_status;
    }

    template <typename ClientType>
    TraceSourceStatus BaseTraceSource<ClientType>::BuildCurrentStatus(const TraceSourceStage stage)
    {
        TraceSourceStatus new_status{};
        new_status.ChangeStage(stage);

        FillProcessInfo(new_status);
        FillDisabledReason(new_status);
        FillCurrentConnections(new_status);
        FillAbortSupported(new_status);

        return new_status;
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::UpdateTraceSourceStatus()
    {
        std::scoped_lock lock(state_mutex_, client_mutex_);

        TraceSourceStatus new_status = BuildCurrentStatus();

        TraceSourceStatusEventArgs args;
        args.new_status = new_status;
        args.old_status = previous_status_;
#if __cplusplus >= 202302L
        args.stack = std::stacktrace::current();
#endif
        PushStatusEvent(args);

        previous_status_ = new_status;
    }

    template <typename ClientType>
    TraceSourceStage BaseTraceSource<ClientType>::GetTraceSourceStage() const
    {
        if (!clients_.empty())
        {
            ClientState highest_state = clients_[0]->client->GetState();
            for (size_t i = 1; i < clients_.size(); ++i)
            {
                highest_state = std::max(highest_state, clients_[i]->client->GetState());
            }

            const auto stage = ClientStateToTraceSourceStage(highest_state);
            return stage;
        }

        if (!unsupported_clients_.empty())
        {
            return TraceSourceStage::kDisabled;
        }

        return TraceSourceStage::kDisconnected;
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::FillProcessInfo(TraceSourceStatus& status)
    {
        std::optional<ClientConnection> conn_info;

        if (!clients_.empty())
        {
            conn_info = clients_.front()->conn_info;
        }
        else if (!unsupported_clients_.empty())
        {
            conn_info = unsupported_clients_.front();
        }
        else if (!pending_clients_.empty())
        {
            conn_info = pending_clients_.front()->conn_info;
        }

        if (conn_info.has_value())
        {
            status.pid              = conn_info->client_pid;
            status.application_name = conn_info->client_name;

            return;
        }

        status.pid              = 0;
        status.application_name = "";
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::FillDisabledReason(TraceSourceStatus& status)
    {
        if (const auto system_disabled_reason = system_disabled_reason_; system_disabled_reason != DisabledReason::kEnabled)
        {
            status.disabled_reason = system_disabled_reason;
            return;
        }

        if (!clients_.empty())
        {
            for (const auto& client : clients_)
            {
                if (client->client->GetState() != ClientState::kDisabled)
                {
                    return;
                }
            }

            status.disabled_reason = client_disabled_reason_;
            return;
        }

        if (!unsupported_clients_.empty())
        {
            status.disabled_reason = DisabledReason::kApiUnsupported;
        }
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::FillCurrentConnections(TraceSourceStatus& status)
    {
        for (const auto& client : clients_)
        {
            if (const ClientState current_state = client->client->GetState();
                current_state == ClientState::kDone || current_state == ClientState::kDisabled || current_state == ClientState::kError)
            {
                continue;
            }

            const ClientConnection& conn_info = client->conn_info;
            status.current_connections.insert({conn_info.umd_connection_id, conn_info.api});
        }
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::FillAbortSupported(TraceSourceStatus& status)
    {
        status.abort_trace_supported = true;
        for (const auto& client : clients_)
        {
            status.abort_trace_supported &= client->client->IsAbortTraceSupported();
        }
    }

    template <typename ClientType>
    typename BaseTraceSource<ClientType>::BoundClient* BaseTraceSource<ClientType>::GetBoundClient(DDConnectionId umd_connection_id)
    {
        std::unique_lock client_lock(client_mutex_);

        const auto& bound_client = FindClient(umd_connection_id);
        if (bound_client == clients_.end())
        {
            return nullptr;
        }

        return bound_client->get();
    }

    template <typename ClientType>
    std::vector<std::unique_ptr<typename BaseTraceSource<ClientType>::BoundClient>>::iterator BaseTraceSource<ClientType>::FindClient(
        DDConnectionId umd_connection_id)
    {
        return std::find_if(clients_.begin(), clients_.end(), [&](const auto& client) { return client->conn_info.umd_connection_id == umd_connection_id; });
    }

    template <typename ClientType>
    std::list<std::unique_ptr<typename BaseTraceSource<ClientType>::BoundClient>>::iterator BaseTraceSource<ClientType>::FindPendingClient(
        DDConnectionId umd_connection_id)
    {
        return std::find_if(
            pending_clients_.begin(), pending_clients_.end(), [&](const auto& client) { return client->conn_info.umd_connection_id == umd_connection_id; });
    }

    template <typename ClientType>
    Result BaseTraceSource<ClientType>::RequestAbortTrace(DDConnectionId umd_connection_id)
    {
        return WithClient<Result>(umd_connection_id, [](auto& client) { return client.RequestAbortTrace(); }).value_or(Result::kFailure);
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::PushStatusEvent(const TraceSourceStatusEventArgs& args)
    {
        {
            std::scoped_lock lock(status_queue_mutex_);
            status_event_queue_.push(args);
        }
        status_queue_cv_.notify_one();
    }

    template <typename ClientType>
    TraceSourceStatusEventArgs BaseTraceSource<ClientType>::PopStatusEvent()
    {
        // Note: caller must hold status_queue_mutex_
        auto args = status_event_queue_.front();
        status_event_queue_.pop();
        return args;
    }

    template <typename ClientType>
    TraceCompletionEventArgs BaseTraceSource<ClientType>::PopCompletionEvent()
    {
        // Note: caller must hold trace_completion_queue_mutex_
        auto args = trace_completion_event_queue_.front();
        trace_completion_event_queue_.pop();
        return args;
    }

    template <typename ClientType>
    TraceCaptureProgressEventArgs BaseTraceSource<ClientType>::PopCaptureProgressEvent()
    {
        // Note: caller must hold capture_progress_queue_mutex_
        auto args = capture_progress_event_queue_.front();
        capture_progress_event_queue_.pop();
        return args;
    }

    template <typename ClientType>
    template <typename T>
    std::optional<T> BaseTraceSource<ClientType>::WithClient(uint16_t client_id, const std::function<T(ClientType&)>& func)
    {
        auto bound_client = GetBoundClient(client_id);
        if (bound_client == nullptr)
        {
            logger_->LogWarning("Client not found {}", 0, client_id, client_id);
            return {};
        }

        return func(*bound_client->client.get());
    }

    template <typename ClientType>
    template <typename T>
    T BaseTraceSource<ClientType>::WithClients(const std::function<T(const std::vector<std::reference_wrapper<ClientType>>&)>& func)
    {
        std::unique_lock client_lock(client_mutex_);

        std::vector<std::reference_wrapper<ClientType>> raw_clients{};
        raw_clients.reserve(clients_.size());

        for (const auto& client : clients_)
        {
            raw_clients.emplace_back(*client->client);
        }

        return func(raw_clients);
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::PerformProcessing([[maybe_unused]] const bool                                                    can_be_aborted,
                                                        [[maybe_unused]] const std::function<void(const std::function<void(float)>&)>& operation)
    {
        std::scoped_lock work_lock(work_mutex_);

        operation([&](float progress) {
            TraceCaptureProgress new_progress;
            new_progress.progress = progress;

            {
                std::scoped_lock client_lock(client_mutex_);
                new_progress.status = BuildCurrentStatus(TraceSourceStage::kProcessing);
                PushStatusEvent({.new_status = new_progress.status, .old_status = {}});
            }

            TraceCaptureProgressEventArgs args;
            args.old_progress = previous_progress_;
            args.new_progress = new_progress;

            {
                std::scoped_lock lock(capture_progress_queue_mutex_);
                capture_progress_event_queue_.push(args);
            }
            capture_progress_queue_cv_.notify_one();
        });
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::TraceCompleted(const TraceCompletionStatus status, const std::string& path, const DDConnectionId umd_connection_id)
    {
        {
            std::scoped_lock lock(trace_completion_queue_mutex_);

            const TraceCompletionResult    result = {.status = status, .path = path, .umd_connection_id = umd_connection_id};
            const TraceCompletionEventArgs args   = {.result = result};
            trace_completion_event_queue_.push(args);
        }
        trace_completion_queue_cv_.notify_one();
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::ReportCaptureProgress(const WritingProgress& progress)
    {
        TraceCaptureProgress new_progress;
        new_progress.num_bytes_dumped    = progress.num_bytes_dumped;
        new_progress.total_bytes_to_dump = progress.total_bytes_to_dump;
        new_progress.progress            = progress.progress;

        {
            std::scoped_lock client_lock(client_mutex_);
            new_progress.status = BuildCurrentStatus(TraceSourceStage::kCapturing);
        }

        TraceCaptureProgressEventArgs args;
        args.old_progress = previous_progress_;
        args.new_progress = new_progress;

        {
            std::scoped_lock lock(capture_progress_queue_mutex_);
            capture_progress_event_queue_.push(args);
            previous_progress_ = new_progress;
        }
        capture_progress_queue_cv_.notify_one();
    }

    template <typename ClientType>
    std::unique_ptr<ByteWriter> BaseTraceSource<ClientType>::GetByteWriter(std::string& path)
    {
        std::unique_ptr<ReadWriteStream> stream = stream_provider_->CreateReadWriteStream(path);
        return std::make_unique<SourceByteWriter>(stream, work_mutex_, [&](const WritingProgress& progress) {
            TraceCaptureProgress new_progress;
            new_progress.num_bytes_dumped    = progress.num_bytes_dumped;
            new_progress.total_bytes_to_dump = progress.total_bytes_to_dump;
            new_progress.progress            = progress.progress;

            {
                std::scoped_lock client_lock(client_mutex_);

                TraceSourceStatus new_status = BuildCurrentStatus(TraceSourceStage::kDumping);

                if (new_status.GetStage() == TraceSourceStage::kDumping)
                {
                    new_status.total_bytes_to_dump = progress.total_bytes_to_dump;
                    new_status.num_bytes_dumped    = progress.num_bytes_dumped;
                    new_status.stage_progress      = progress.progress;
                }

                new_progress.status = new_status;
            }

            TraceCaptureProgressEventArgs args;
            args.old_progress = previous_progress_;
            args.new_progress = new_progress;

            {
                std::scoped_lock lock(capture_progress_queue_mutex_);
                capture_progress_event_queue_.push(args);
                previous_progress_ = new_progress;
            }
            capture_progress_queue_cv_.notify_one();
        });
    }

    template <typename ClientType>
    std::unique_ptr<RdfWriter> BaseTraceSource<ClientType>::GetRdfWriter(std::string& path)
    {
        std::unique_ptr<ReadWriteStream> stream = stream_provider_->CreateReadWriteStream(path);
        return std::make_unique<SourceRdfWriter>(stream, work_mutex_, [&](const WritingProgress& progress) {
            TraceCaptureProgress new_progress;
            new_progress.num_bytes_dumped    = progress.num_bytes_dumped;
            new_progress.total_bytes_to_dump = progress.total_bytes_to_dump;
            new_progress.progress            = progress.progress;

            {
                std::scoped_lock client_lock(client_mutex_);

                TraceSourceStatus new_status = BuildCurrentStatus(TraceSourceStage::kDumping);

                if (new_status.GetStage() == TraceSourceStage::kDumping)
                {
                    new_status.total_bytes_to_dump = progress.total_bytes_to_dump;
                    new_status.num_bytes_dumped    = progress.num_bytes_dumped;
                    new_status.stage_progress      = progress.progress;
                }

                new_progress.status = new_status;
            }

            TraceCaptureProgressEventArgs args;
            args.old_progress = previous_progress_;
            args.new_progress = new_progress;

            {
                std::scoped_lock lock(capture_progress_queue_mutex_);
                capture_progress_event_queue_.push(args);
                previous_progress_ = new_progress;
            }
            capture_progress_queue_cv_.notify_one();
        });
    }

    template <typename ClientType>
    const BaseTraceSource<ClientType>::ClientConfigType& BaseTraceSource<ClientType>::GetClientConfig()
    {
        return GetConfig();
    }

    template <typename ClientType>
    const std::shared_ptr<Logger>& BaseTraceSource<ClientType>::GetLogger()
    {
        return logger_;
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::SetDisabledReason(const DisabledReason reason)
    {
        system_disabled_reason_ = reason;
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::RegisterStatusEvent(const TraceSourceStatusEvent& event)
    {
        std::scoped_lock lock(status_events_mutex_);
        status_events_.emplace_back(event);
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::RegisterTraceCompletionEvent(const TraceCompletionEvent& event)
    {
        std::scoped_lock lock(trace_completion_events_mutex_);
        trace_completion_events_.emplace_back(event);
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::RegisterTraceCaptureProgressEvent(const TraceCaptureProgressEvent& event)
    {
        std::scoped_lock lock(capture_progress_events_mutex_);
        capture_progress_events_.emplace_back(event);
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::QueryStatus()
    {
        std::scoped_lock lock(state_mutex_, client_mutex_, status_events_mutex_);

        TraceSourceStatus new_status = BuildCurrentStatus();

        // Update all listeners with new trace status
        for (auto& [listener, callback] : status_events_)
        {
            if (callback)
            {
                callback(listener, {.new_status = new_status, .old_status = previous_status_});
            }
        }
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::SetAllClientsDisabledReason(const DisabledReason reason)
    {
        if (reason != DisabledReason::kEnabled)
        {
            client_disabled_reason_ = reason;
            UpdateTraceSourceStatus();
        }
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::SetApiFilter(const std::unordered_set<Api>& apis)
    {
        supported_apis_ = apis;
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::SetSerializeConnectionInit(const bool serialize_connection_init)
    {
        serialize_connection_init_ = serialize_connection_init;
    }

    template <typename ClientType>
    void BaseTraceSource<ClientType>::SetNoCrashDetectedCallback(std::function<void()> callback)
    {
        no_crash_detected_callback_ = std::move(callback);
    }
}  // namespace devtrace
