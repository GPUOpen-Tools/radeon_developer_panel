// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for base trace source.

#ifndef RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_BASE_TRACE_SOURCE_V2_H_
#define RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_BASE_TRACE_SOURCE_V2_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <semaphore>
#include <shared_mutex>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include <dd_connection_api.h>

#include "client_connection_manager.h"

#include "dev_trace_common.h"
#include "file_writing.h"
#include "logging.h"
#include "overlay_manager.h"
#include "trace_source.h"

#include "trace_source_client.h"

namespace devtrace
{
    template <typename ClientType>
    struct IClientEventBindingDelegate
    {
        virtual ~IClientEventBindingDelegate() = default;

        virtual void Bind(ClientType* client) = 0;
    };

    /// @brief The base implementation of a trace source.
    ///
    /// This handles the state of the trace source, providing file writers and the client lifecycle.
    /// @tparam ClientType The type of client this trace source uses.
    template <typename ClientType>
    class BaseTraceSource : public ClientConnectionSubscriberV2, public ClientUtils<typename ClientType::ClientConfigType>
    {
    public:
        using ClientConfigType  = ClientType::ClientConfigType;  ///< The config type for the trace source / client.
        using ClientFactoryType = ClientFactory<ClientType>;     ///< The factory type that will create clients.

    private:
        /// @brief Internal class that stores a client bound to a trace source.
        struct BoundClient
        {
            /// @brief Constructor.
            /// @param [in] conn_info The connection information for the client.
            /// @param [in] client The client.
            BoundClient(ClientConnection conn_info, std::unique_ptr<ClientType>& client)
                : conn_info(std::move(conn_info))
                , client(std::move(client))
            {
            }

            ClientConnection            conn_info;  ///< The connection information for the client.
            std::unique_ptr<ClientType> client;     ///< The client.
        };

    public:
        /// @brief Constructor.
        /// @param [in] client_factory The factory that can be used to create clients.
        /// @param [in] stream_provider An object that provides streams.
        /// @param [in] overlay_manager Manager for the developer overlay.
        /// @param [in] overlay_feature The feature of the overlay to use.
        /// @param [in] logger Object used for logging.
        /// @param [in] binding_delegate An optional delegate to do additional binding to clients.
        BaseTraceSource(const std::shared_ptr<ClientFactoryType>&       client_factory,
                        const std::shared_ptr<ReadWriteStreamProvider>& stream_provider,
                        const std::shared_ptr<OverlayManager>&          overlay_manager,
                        OverlayFeature                                  overlay_feature,
                        const std::shared_ptr<Logger>&                  logger,
                        IClientEventBindingDelegate<ClientType>*        binding_delegate = nullptr);

        /// @brief Destructor.
        ~BaseTraceSource() override;

        /// @brief Callback responds to client state change.
        /// @param [in] object The listener object.
        /// @param [in] args The client state event args.
        static void OnClientStateChangeEvent(void* object, const ClientStateChangeEventArgs& args);

        /// @brief Gets the config used for the trace source / clients.
        /// @return The config used for the trace source / clients.
        ClientConfigType& GetConfig();

        void OnDriverConnected(const DDConnectionInfo& connection_info) override;
        void OnDriverDisconnected(DDConnectionId umd_connection_id) override;
        void OnDriverStateChanged(DDConnectionId umd_connection_id, DD_DRIVER_STATE state) override;

        void EmitPostProcessEvent(const std::string& progress_text, float progress) override;

        void TraceStatusThreadFunc();

        void CaptureProgressThreadFunc();

        void TraceCompletionThreadFunc();

    private:
        /// @brief Initializes a pending client with the umd connection id if needed.
        /// @param [in] umd_connection_id The UMD connection id of the client to initialize.
        /// @param [in] state The current state of the driver.
        void InitClientIfNeeded(DDConnectionId umd_connection_id, DD_DRIVER_STATE state);

        /// @brief Handles the developer mode overlay for the client.
        /// @param [in] bound_client The client to handle the overlay for.
        void HandleDeveloperModeOverlay(const std::unique_ptr<BoundClient>& bound_client);

        /// @brief Updates the status of the trace source.
        ///
        /// The trace source status is used in displaying the state of a client
        /// in the UI. Such as when a client is "Ready" or "Unsupported" for a feature.
        void UpdateTraceSourceStatus();

        /// @brief Builds a TraceSourceStatus snapshot.
        /// @note The caller must hold client_mutex_.
        /// @return The current trace source status.
        TraceSourceStatus BuildCurrentStatus();

        /// @brief Builds a TraceSourceStatus snapshot with an explicit stage override.
        /// @note The caller must hold client_mutex_.
        /// @param [in] stage The stage to set on the status.
        /// @return The current trace source status with the given stage.
        TraceSourceStatus BuildCurrentStatus(TraceSourceStage stage);

        /// @brief Gets the current trace source stage based on the connected clients and state of the trace source.
        /// @return The current trace source stage.
        TraceSourceStage GetTraceSourceStage() const;

        /// @brief Fills in the current process information.
        /// @param [out] status The status to fill.
        void FillProcessInfo(TraceSourceStatus& status);

        /// @brief Fills in the disabled reason.
        /// @param [out] status The status to fill.
        void FillDisabledReason(TraceSourceStatus& status);

        /// @brief Fills in the current usable connections.
        /// @param [out] status The status to fill.
        void FillCurrentConnections(TraceSourceStatus& status);

        /// @brief Fills in whether abort trace is supported.
        /// @param [out] status The status to fill.
        void FillAbortSupported(TraceSourceStatus& status);

        /// @brief Finds a bound client with the given UMD connection id.
        /// @param [in] umd_connection_id The UMD of the client to find.
        /// @return A pointer to bound client or nullptr if not found.
        BoundClient* GetBoundClient(DDConnectionId umd_connection_id);

        /// @brief Finds the client with the given UMD connection id.
        /// @param [in] umd_connection_id The UMD of the client to find.
        /// @return An iterator pointing to the client.
        std::vector<std::unique_ptr<BoundClient>>::iterator FindClient(DDConnectionId umd_connection_id);

        /// @brief Finds the pending client with the given UMD connection id.
        /// @param [in] umd_connection_id The UMD of the client to find.
        /// @return An iterator pointing to the client.
        std::list<std::unique_ptr<BoundClient>>::iterator FindPendingClient(DDConnectionId umd_connection_id);

    public:
        /// @brief Requests that the current trace be aborted.
        /// @param [in] umd_connection_id The UMD connection id of the client to abort the trace for.
        /// @return The result of the abortion.
        Result RequestAbortTrace(DDConnectionId umd_connection_id);

    protected:
        void PushStatusEvent(const TraceSourceStatusEventArgs& args);

        TraceSourceStatusEventArgs PopStatusEvent();

        TraceCompletionEventArgs PopCompletionEvent();

        TraceCaptureProgressEventArgs PopCaptureProgressEvent();

        /// @brief Performs an operation with the specified client.
        /// @tparam T The result type of the operation.
        /// @param [in] client_id The id of the client to perform the operation with from the status.
        /// @param [in] func The operation to perform with the client.
        /// @return An optional that will be empty if the client wasn't found or have the result of func applied to the client.
        template <typename T>
        std::optional<T> WithClient(uint16_t client_id, const std::function<T(ClientType&)>& func);

        /// @brief Performs an operation with all connected clients.
        /// @tparam T The result type of the operation.
        /// @param [in] func The operation to perform with all the clients.
        /// @return The result of the operation.
        template <typename T>
        T WithClients(const std::function<T(const std::vector<std::reference_wrapper<ClientType>>&)>& func);

    public:
        /// @brief Performs processing in the same thread that it is called.
        /// @param [in] can_be_aborted true if the processing can be aborted, false otherwise.
        /// @param [in] operation The operation to perform, the first argument is a function that can be used to report progress.
        void PerformProcessing(bool can_be_aborted, const std::function<void(const std::function<void(float)>&)>& operation);

        void                           TraceCompleted(TraceCompletionStatus status, const std::string& path, DDConnectionId umd_connection_id) override;
        void                           ReportCaptureProgress(const WritingProgress& progress) override;
        std::unique_ptr<ByteWriter>    GetByteWriter(std::string& path) override;
        std::unique_ptr<RdfWriter>     GetRdfWriter(std::string& path) override;
        const ClientConfigType&        GetClientConfig() override;
        const std::shared_ptr<Logger>& GetLogger() override;

        /// @brief Sets the system disabled reason for this trace source
        /// @param [in] reason The disabled reason
        void SetDisabledReason(DisabledReason reason);

        /// @brief Registers an event listener for trace source status update.
        /// @param [in] event The event structure containing listener and callback function.
        void RegisterStatusEvent(const TraceSourceStatusEvent& event);

        /// @brief Registers an event listener for trace completion events.
        /// @param [in] event The event structure containing the listener and callback function.
        void RegisterTraceCompletionEvent(const TraceCompletionEvent& event);

        /// @brief Registers an event listener for trace capture progress events.
        /// @param [in] event The event structure containing the listener and callback function.
        void RegisterTraceCaptureProgressEvent(const TraceCaptureProgressEvent& event);

        /// @brief Forces a broadcast of the latest trace source status
        void QueryStatus();

        /// @brief Sets the reason to specify in the trace source state when all clients are disabled.
        /// @param [in] reason The reason to store in the trace source state when all clients are disabled.
        void SetAllClientsDisabledReason(DisabledReason reason);

        /// @brief Sets the supported APIs.
        /// @param [in] apis The supported APIs.
        void SetApiFilter(const std::unordered_set<Api>& apis);

        /// @brief Sets whether initialization of new connections should be serialized or not.
        /// @param [in] serialize_connection_init true if connection initialization should be serialized.
        void SetSerializeConnectionInit(bool serialize_connection_init);

        /// @brief Sets the callback to invoke when no crash is detected.
        /// @param [in] callback The callback to invoke.
        void SetNoCrashDetectedCallback(std::function<void()> callback);

    private:
        std::shared_ptr<ClientFactoryType>       client_factory_;   ///< Factory that makes clients.
        std::shared_ptr<ReadWriteStreamProvider> stream_provider_;  ///< Provides streams to write traces.
        std::shared_ptr<OverlayManager>          overlay_manager_;  ///< Manager for the developer overlay.
        OverlayFeature                           overlay_feature_;  ///< The feature of the overlay to use.

    protected:
        std::shared_ptr<Logger> logger_;  ///< Object used for logging.
        std::atomic_bool        is_processing_ = false;

    private:
        IClientEventBindingDelegate<ClientType>*  binding_delegate_;                   ///< An optional delegate to do additional binding to clients.
        std::mutex                                state_mutex_;                        ///< Mutex that guards the internal state of the trace source.
        std::mutex                                client_mutex_;                       ///< Mutex that guards access to the client data.
        std::mutex                                init_mutex_;                         ///< Mutex that guards init if serialize connections is true.
        std::atomic_bool                          serialize_connection_init_ = false;  ///< true if connection initialization should be serialized.
        std::list<std::unique_ptr<BoundClient>>   pending_clients_;                    ///< Clients that have connected but not yet been initialized.
        std::vector<std::unique_ptr<BoundClient>> clients_;                            ///< Clients that have been initialized.
        std::unordered_set<Api>                   supported_apis_;                     ///< The current supported APIs.
        std::list<ClientConnection>               unsupported_clients_;                ///< Clients that connected but did not pass the API filter.
        DisabledReason                            system_disabled_reason_;             ///< Reason that the system is disabled.
        ClientConfigType                          config_{};                           ///< The configuration for the trace source.
        WritingProgress                           writing_progress_;                   ///< The progress of writing a file.

        /// @brief Details about the state of processing.
        struct ProcessingState
        {
            bool is_processing = false;  ///< true if processing is occurring, false otherwise.
            bool can_abort     = false;  ///< true if the processing can be aborted, false otherwise.

            /// @brief Equality operator for processing state.
            /// @param [in] other The state to compare against.
            /// @return true if the states are equal.
            bool operator==(const ProcessingState& other)
            {
                return is_processing == other.is_processing && can_abort == other.can_abort;
            }
        };

        float           processing_progress_{};  ///< The current progress of post-processing.
        ProcessingState processing_state_;       ///< The post-processing state.
        std::mutex      work_mutex_;             ///< Mutex that guards writing files and processing.

        TraceCompletionResult completion_result_;                                   ///< The result of the completed trace.
        std::mutex            completion_mutex_;                                    ///< Mutex that guards trace completed.
        DisabledReason        client_disabled_reason_ = DisabledReason::kNoReason;  ///< The reason to display if all the clients are disabled.

#pragma region TraceCompletionResults
        std::thread                          trace_completion_thread_;              ///< Thread to handle reporting of trace completion.
        std::atomic_bool                     trace_completion_thread_exit_{false};  ///< Flag to signal exit of trace completion reporting.
        std::mutex                           trace_completion_queue_mutex_;         ///< Mutex that guards the trace completion queue.
        std::condition_variable              trace_completion_queue_cv_;            ///< Condition variable to signal trace completion queue has work.
        std::queue<TraceCompletionEventArgs> trace_completion_event_queue_;         ///< Queue of trace completion events.
        std::mutex                           trace_completion_events_mutex_;        ///< Mutex that guards the list of trace completion events.
        std::vector<TraceCompletionEvent>    trace_completion_events_;              ///< List of registered completion events.
#pragma endregion

#pragma region Capture Progress Reporting
        TraceCaptureProgress                      previous_progress_{};                  ///< Previous progress step cache.
        std::thread                               capture_progress_thread_;              ///< Thread to handle reporting of capture progress events.
        std::atomic_bool                          capture_progress_thread_exit_{false};  ///< Flag to signal exit of progress reporting.
        std::mutex                                capture_progress_queue_mutex_;         ///< Mutex that guards progress event queue.
        std::condition_variable                   capture_progress_queue_cv_;            ///< Condition variable to signal capture progress queue has work.
        std::queue<TraceCaptureProgressEventArgs> capture_progress_event_queue_;         ///< Queue of progress update events to process.
        std::mutex                                capture_progress_events_mutex_;        ///< Mutex that guards the list of capture progress events.
        std::vector<TraceCaptureProgressEvent>    capture_progress_events_;              ///< List of registered capture progress events.
#pragma endregion

#pragma region Post-Processing Progress Reporting
        std::binary_semaphore post_processing_progress_thread_exit_{0};  ///< Semaphore to signal exit of post-processing progress reporting.
        std::mutex            post_processing_progress_queue_mutex_;     ///< Mutex that guards post-processing progress event queue.
#pragma endregion

#pragma region Status Reporting
        std::thread                            status_thread_;              ///< Thread to handle status reporting
        TraceSourceStatus                      previous_status_;            ///< Cache previous source status.
        std::mutex                             status_events_mutex_;        ///< Mutex guarding access to status event registry.
        std::vector<TraceSourceStatusEvent>    status_events_;              ///< Registered events for status update.
        std::atomic_bool                       status_thread_exit_{false};  ///< Flag to signal exit of status reporting.
        std::mutex                             status_queue_mutex_;         ///< Mutex that guards status event queue.
        std::condition_variable                status_queue_cv_;            ///< Condition variable to signal status queue has work.
        std::queue<TraceSourceStatusEventArgs> status_event_queue_;         ///< Queue of status update events to process.
#pragma endregion

#pragma region No Crash Detected Callback
        std::function<void()> no_crash_detected_callback_;  ///< Callback for no crash detected.
#pragma endregion
    };

}  // namespace devtrace

#include "base_trace_source.inl"

#endif
