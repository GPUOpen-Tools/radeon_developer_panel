// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for UberTrace trace source client.

#ifndef RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_UBERTRACE_CLIENT_H_
#define RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_UBERTRACE_CLIENT_H_

#include <atomic>
#include <thread>

#include <dd_uber_trace_api.h>

#include "../cancellable_timer.h"
#include "chunk_writing.h"
#include "trace_source_client.h"
#include "ubertrace_features.h"
#include "ubertrace_params.h"

namespace devtrace
{
    using UbertraceUserId = uint32_t;                                          ///< The type for UberTrace orchestrator user ids.
    using DisabledUsers   = std::unordered_map<UbertraceUserId, ClientState>;  ///< List of disabled users and their disabled states.
    static constexpr UbertraceUserId kInvalidUbertraceUserId = 0;              ///< Invalid UberTrace orchestrator user id.

    /// @brief The state of an UbertraceOrchestrator
    struct UbertraceOrchestratorState
    {
        ClientState     state;
        UbertraceUserId relevant_user;

        DisabledUsers disabled_users;  ///< Users that tracing is disabled for.
    };

    struct UbertraceOrchestratorStateChangedEventArgs
    {
        UbertraceUserId user_id;
        ClientState     state;
    };

    struct UbertraceOrchestratorStateChangedEvent
    {
        void*                                                                         listener;  ///< event listener object.
        std::function<void(void*, const UbertraceOrchestratorStateChangedEventArgs&)> callback;  ///< event callback.
    };

    /// @brief The type of capture that is being executed.
    enum UberTraceCaptureType : uint8_t
    {
        kUberTraceCaptureTypeNormal = 0,
        kUberTraceCaptureTypeAutoCapture,
        kUberTraceCaptureTypeDelayedAutoCapture
    };

    /// @brief Configuration for an UberTrace capture.
    struct UbertraceCaptureConfig
    {
        const uint32_t             capture_mode = 0;  ///< Up to interpretation of the implementing client, but 0 is always valid (default).
        const UberTraceCaptureType capture_type = kUberTraceCaptureTypeNormal;  ///< The type of capture.

        UberTraceConfig                         ubertrace_config;  ///< The config to be serialized and sent to UberTrace.
        std::function<void(const std::string&)> on_completed;      ///< A function to call when the trace completes successfully.
    };

    /// @brief Dependencies for requesting an UberTrace capture.
    struct UbertraceCaptureDeps
    {
        std::shared_ptr<AdditionalChunkWriter> additional_chunk_writer;  ///< Object used for system info writing.
        ProcessInfoChunk                       process_info_chunk;       ///< Process info chunk.

        std::function<void()>                       tracing_started;  ///< Called when tracing is started.
        std::function<void()>                       tracing_ended;    ///< Called when tracing is ended.
        std::function<void(const WritingProgress&)> report_progress;  ///< Called to report progress during trace capture.
    };

    struct UbertraceAutoCaptureConfig
    {
        uint32_t delay_ms     = 0;  ///< The time delay for the auto capture. A delay of zero will just be started immediately.
        uint32_t capture_mode = 0;  ///< The capture mode to use.
    };

    /// @brief Pseudo-client implementation that allows multiple users to use UberTrace over one UMD connection.
    class UbertraceOrchestrator : public std::enable_shared_from_this<UbertraceOrchestrator>
    {
        static constexpr uint32_t kDumpTimeoutMs      = 1000;  ///< The timeout for dumping a trace.
        static constexpr uint32_t kCollectTracePollMs = 200;   ///< The interval to poll for trace completion.

        /// @brief The state of an UbertraceOrchestrator
        struct UbertraceOrchestratorInternalState
        {
            ClientState     state;          ///< The state of the client.
            UbertraceUserId relevant_user;  ///< The user that currently has exclusive control over the orchestrator.
        };

    public:
        /// @brief Constructor.
        /// @param [in] conn_info The connection info for the client.
        /// @param [in] ubertrace_api The UberTrace API.
        /// @param [in] features the UberTrace features.
        UbertraceOrchestrator(ClientConnection conn_info, DDUberTraceApi* ubertrace_api, const UbertraceFeatures& features);

        /// @brief Gets a new user.
        /// @return The new user.
        std::unique_ptr<class UbertraceUser> GetNewUser();

        /// @brief Initializes UberTrace.
        /// @param [in] user_id The userid to initialize.
        /// @param [in] enable_tracing true if tracing should be enabled, false otherwise.
        /// @return The result of initialization.
        Result Initialize(UbertraceUserId user_id, bool enable_tracing);

        static void OnWritingStatusEvent(void* object, const WritingStatusEventArgs& args);

        void RegisterStateChangedEvent(UbertraceUserId user_id, const UbertraceOrchestratorStateChangedEvent& event);

        void EmitStateChangedEvent(UbertraceUserId user_id, const UbertraceOrchestratorStateChangedEventArgs& args);

    private:
        /// @brief Actually performs the UberTrace init.
        /// @param [in] user_id The user to init.
        /// @return The result of the init.
        Result DoInit(UbertraceUserId user_id);

        /// @brief Enables tracing with UberTrace.
        /// @param [in] user_id The user to enable tracing for.
        /// @return The result of enabling tracing.
        Result EnableTracing(UbertraceUserId user_id);

    public:
        /// @brief De-inits if all the users have called this function.
        /// @param [in] user_id The user to de-init.
        void Disconnect(UbertraceUserId user_id);

        /// @brief Adds some preliminary sources that are sent to UberTrace during post device init.
        /// @param [in] sources The preliminary sources to send to UberTrace.
        void AddPreliminarySources(std::vector<UberTraceSource>& sources);

        /// @brief Adds early configuration (global params) to be sent during preliminary setup.
        /// @param [in] early_config The early configuration to merge with preliminary sources.
        void AddEarlyConfiguration(UberTraceConfig& early_config);

        /// @brief Handles the driver state.
        /// @param [in] user_id The user handling the driver state.
        /// @param [in] state The state to handle.
        /// @param [in] client_utils Client utils to use.
        /// @return The result of handling the driver state.
        Result HandleDriverState(UbertraceUserId user_id, DD_DRIVER_STATE state, BaseClientUtils& client_utils);

        /// @brief Cancels the current trace.
        /// @param [in] user_id The user that is requesting the abort.
        /// @return The result of aborting the current trace.
        Result RequestAbortTrace(UbertraceUserId user_id);

        /// @brief Returns true if abort is supported, false otherwise.
        /// @param [in] user_id The user to check abort support for.
        /// @return true if abort is supported, false otherwise.
        bool IsAbortTraceSupported(UbertraceUserId user_id) const;

        /// @brief Prepares for a delayed capture by transitioning the state.
        /// @param [in] user_id The user initiating the capture.
        /// @return The result of the preparation.
        Result PrepareForDelayedCapture(UbertraceUserId user_id);

        /// @brief Requests that a trace be taken.
        /// @param [in] user_id The user initiating the trace.
        /// @param [in] capture_config The configuration for the capture.
        /// @param [in] client_utils The client utils to get a byte writer from.
        /// @param [in] deps The dependencies required for requesting a trace.
        /// @return The result of the request.
        Result RequestBeginTrace(UbertraceUserId user_id, UbertraceCaptureConfig& capture_config, BaseClientUtils& client_utils, UbertraceCaptureDeps deps);

    private:
        /// @brief Returns whether a capture can be taken for the given user.
        /// @param [in] user_id The user attempting to take a capture.
        /// @return true if a capture can be taken, false otherwise.
        bool IsReadyForCapture(UbertraceUserId user_id) const;

        /// @brief Calls UberTrace and updates the configuration.
        /// @param [in] ubertrace_config The configuration to send to UberTrace.
        /// @param [in] client_utils The client utils to use for logging.
        /// @return The result of updating the configuration.
        Result UpdateConfiguration(UberTraceConfig& ubertrace_config, BaseClientUtils& client_utils);

        /// @brief Polls for a trace being complete.
        /// @param [in] client_utils The client utils to get a byte writer from.
        /// @param [in] on_completed Function to call on a successful dump.
        /// @param [in] is_auto_capture true if the capture was an auto capture.
        /// @param [in] deps The dependencies required for requesting a trace.
        void PollForTraceCollectionFinished(BaseClientUtils&                               client_utils,
                                            const std::function<void(const std::string&)>& on_completed,
                                            bool                                           is_auto_capture,
                                            const UbertraceCaptureDeps&                    deps);

        /// @brief Handles a successful trace.
        /// @param [in] user_id The user that initiated the trace.
        /// @param [in] client_utils The client utils to use for logging.
        /// @param [in] on_completed Function to call on a successful dump.
        /// @param [in] is_auto_capture true if the capture was an auto capture.
        /// @param [in] path The path of the trace.
        /// @param [in] additional_chunk_result The result of writing the additional chunks.
        void HandleSuccessfulTrace(UbertraceUserId                                user_id,
                                   BaseClientUtils&                               client_utils,
                                   const std::function<void(const std::string&)>& on_completed,
                                   bool                                           is_auto_capture,
                                   const std::string&                             path,
                                   Result                                         additional_chunk_result);

        /// @brief Handles an unsuccessful trace.
        /// @param [in] user_id The user that initiated the trace.
        /// @param [in] client_utils The client utils to use for logging.
        /// @param [in] is_auto_capture true if the capture was an auto capture.
        void HandleUnsuccessfulTrace(UbertraceUserId user_id, BaseClientUtils& client_utils, bool is_auto_capture);

        /// @brief Aborts the currently running trace.
        bool AbortTrace();

        /// @brief Attempts to collect the trace
        /// @param [in] client_utils The client utils to get a byte writer from.
        /// @param [in] deps The dependencies required for requesting a trace.
        /// @param [out] path The path where the trace was written.
        /// @param [out] additional_chunk_result The result of writing the additional chunks.
        /// @param [out] wrote_file true if writing started, false otherwise.
        DD_RESULT CollectTrace(BaseClientUtils&            client_utils,
                               const UbertraceCaptureDeps& deps,
                               std::string&                path,
                               Result&                     additional_chunk_result,
                               bool&                       wrote_file);

    public:
        /// @brief Sets whether a user is disabled or not.
        /// @param [in] user_id The user to change the disabled status for.
        /// @param [in] disabled true if the user should be disabled, false otherwise.
        void SetUserIsDisabled(UbertraceUserId user_id, bool disabled);

    private:
        /// @brief Sets whether a user is disabled or not.
        /// @param [in] user_id The user to change the disabled status for.
        /// @param [in] disabled true if the user should be disabled, false otherwise.
        /// @param [in] state The state to disable the user with, will only enable the user if the state matches.
        void SetUserIsDisabled(UbertraceUserId user_id, bool disabled, ClientState state);

    public:
        /// @brief Gets the UberTrace features.
        /// @return The Ubertrace features.
        const UbertraceFeatures& GetFeatures() const;

    private:
        /// @brief Set the state for an Ubertrace user.
        /// @param [in] user_id The user to set the state for.
        /// @param [in] state The new state for the user.
        /// @param [in] disabled_users The list of disabled users.
        void SetUserState(UbertraceUserId user_id, ClientState state, const DisabledUsers& disabled_users);

        ClientState GetUserState(UbertraceUserId user_id);

        void SetState(ClientState new_state, UbertraceUserId relevant_user, const DisabledUsers& disabled_users);

        ClientConnection                           conn_info_{};                ///< The connection info for the client.
        DDUberTraceApi*                            ubertrace_api_ = nullptr;    ///< The UberTrace API.
        UbertraceFeatures                          features_;                   ///< The UberTrace features.
        std::recursive_mutex                       disabled_user_mutex_;        ///< The mutex that guards the disabled users.
        DisabledUsers                              disabled_users_;             ///< The list of disabled users.
        std::map<UbertraceUserId, DD_DRIVER_STATE> latest_driver_states_;       ///< The latest driver state encountered per user.
        std::vector<UberTraceSource>               preliminary_sources_;        ///< The preliminary sources to send during post device init.
        std::unique_ptr<UberTraceGlobalParams>     preliminary_global_params_;  ///< The preliminary global params to send during post device init.
        std::atomic<UbertraceUserId>               current_user_id_;            ///< The current user id.
        std::atomic<UbertraceUserId>               capturing_user_ = kInvalidUbertraceUserId;  ///< The currently capturing user.
        std::unordered_set<UbertraceUserId>        initialized_users_;                         ///< The list of users that have called Initialize().
        bool                                       enabled_tracing_ = false;                   ////< true if tracing has been enabled, false otherwise.
        std::atomic<bool>                          should_poll_for_trace_;  ///< true if the trace polling thread should continue to poll for a completed trace.
        std::thread                                trace_polling_thread_;   ///< The thread that polls for a completed trace.
        std::mutex                                 state_changed_event_mutex_;  ///< Mutex protecting statue event list.
        std::recursive_mutex                       user_state_mutex_;           ///< The mutex that guards the state of this client.
        std::map<UbertraceUserId, ClientState>     user_state_;                 ///< User
        std::map<UbertraceUserId, std::vector<UbertraceOrchestratorStateChangedEvent>> state_changed_events_;  ///< List of registered status event listeners.
    };

    struct UbertraceUserStateChangeEventArgs
    {
        ClientState new_state;
        ClientState old_state;
    };

    struct UbertraceUserStateChangeEvent
    {
        void*                                                                listener = nullptr;  ///< event listener object.
        std::function<void(void*, const UbertraceUserStateChangeEventArgs&)> callback;            ///< event callback.
    };

    /// @brief User of an UbertraceOrchestrator.
    class UbertraceUser : public std::enable_shared_from_this<UbertraceUser>
    {
    public:
        static void OnOrchestratorStateChangedEvent(void* listener, const UbertraceOrchestratorStateChangedEventArgs& args);

        /// @brief Constructor.
        /// @param [in] orchestrator The orchestrator to use.
        /// @param [in] user_id The user id of this user.
        UbertraceUser(const std::shared_ptr<UbertraceOrchestrator>& orchestrator, UbertraceUserId user_id);

        /// @brief Initializes UberTrace.
        /// @param [in] enable_tracing true if tracing should be enabled, false otherwise.
        /// @return The result of initialization.
        Result Initialize(bool enable_tracing) const;

        /// @brief Disconnects this user
        void Disconnect() const;

        /// @brief Adds some preliminary sources that are sent to UberTrace during post device init.
        /// @param [in] sources The preliminary sources to send to UberTrace.
        void AddPreliminarySources(std::vector<UberTraceSource>& sources) const;

        /// @brief Adds early configuration (global params) to be sent during preliminary setup.
        /// @param [in] early_config The early configuration to merge with preliminary sources.
        void AddEarlyConfiguration(UberTraceConfig& early_config) const;

        /// @brief Handles the driver state.
        /// @param [in] state The state to handle.
        /// @param [in] client_utils Client utils to use.
        /// @return The result of handling the driver state.
        Result HandleDriverState(DD_DRIVER_STATE state, BaseClientUtils& client_utils) const;

        /// @brief Cancels the current trace.
        /// @return The result of aborting the current trace.
        Result RequestAbortTrace() const;

        /// @brief Returns true if abort is supported, false otherwise.
        /// @return true if abort is supported, false otherwise.
        bool IsAbortTraceSupported() const;

        /// @brief Gets the state of this user.
        /// @return The state of this user.
        ClientState GetState() const;

        /// @brief Prepares for a delayed capture by transitioning the state.
        /// @return The result of the preparation.
        Result PrepareForDelayedCapture() const;

        /// @brief Requests that a trace be taken.
        /// @param [in] capture_config The configuration for the capture.
        /// @param [in] client_utils The client utils to get a byte writer from.
        /// @param [in] deps The dependencies required for requesting a trace.
        /// @return The result of the request.
        Result RequestBeginTrace(UbertraceCaptureConfig& capture_config, BaseClientUtils& client_utils, UbertraceCaptureDeps deps) const;

        /// @brief Sets whether tracing is disabled or not.
        /// @param [in] disabled true if tracing should be disabled, false otherwise.
        void SetIsDisabled(bool disabled) const;

        /// @brief Gets the UberTrace features.
        /// @return The Ubertrace features.
        const UbertraceFeatures& GetFeatures() const;

        /// @brief Registers a callback for state changed event.
        /// @param [in] event The state changed event callback.
        void RegisterStateChangedEvent(const UbertraceUserStateChangeEvent& event);

        /// @brief Emits a state changed event to registered listener.
        /// @param [in] args The event args.
        void EmitStateChangedEvent(const UbertraceUserStateChangeEventArgs& args);

    private:
        std::shared_ptr<UbertraceOrchestrator> orchestrator_;                       ///< The orchestrator to use.
        UbertraceUserId                        user_id_ = kInvalidUbertraceUserId;  ///< The user id of this user.
        ClientState                            state_;                              ///< The state of this user.
        std::mutex                             state_changed_event_mutex_;          ///< Mutex which guards state changed event.
        UbertraceUserStateChangeEvent          state_changed_event_;                ///< The state changes broadcast event.
    };

    /// @brief Client implementation for UberTrace.
    /// @tparam ConfigType The client config type.
    template <typename ConfigType>
    class UbertraceClient : public TriggerableClient<ConfigType>
    {
    public:
        static void OnUbertraceUserStateChangedEvent(void* object, const UbertraceUserStateChangeEventArgs& args);

        /// @brief Constructor.
        /// @param [in] conn_info The connection information for the client.
        /// @param [in] client_utils The utils that the client can use.
        /// @param [in] user The UberTrace user to use.
        /// @param [in] additional_chunk_writer Object used for system info writing.
        /// @param [in] enable_tracing true if this client should enable tracing, false otherwise.
        UbertraceClient(ClientConnection                              conn_info,
                        ClientUtils<ConfigType>&                      client_utils,
                        std::unique_ptr<UbertraceUser>                user,
                        const std::shared_ptr<AdditionalChunkWriter>& additional_chunk_writer,
                        bool                                          enable_tracing = false);

        /// @brief Destructor.
        ~UbertraceClient() override = default;

        UbertraceClient(const UbertraceClient&)             = delete;
        UbertraceClient(const UbertraceClient&&)            = delete;
        UbertraceClient& operator=(const UbertraceClient&)  = delete;
        UbertraceClient& operator=(const UbertraceClient&&) = delete;

        DD_DRIVER_STATE GetInitDriverState() override;
        Result          Initialize() override;
        void            Disconnect() override;

        Result HandleDriverState(DD_DRIVER_STATE state) override;

    private:
        /// @brief Begins the auto capture.
        /// @param [in] config The auto capture config.
        /// @return The result of starting the capture.
        Result BeginAutoCapture(const UbertraceAutoCaptureConfig& config);

    protected:
        /// @brief Gets the preliminary UberTrace sources for the client.
        /// @param [out] sources The preliminary sources for the client.
        virtual void GetPreliminarySources(std::vector<UberTraceSource>& sources);

        /// @brief Gets the early configuration to send during preliminary setup.
        /// This allows configuring global driver parameters that need to be set before capture.
        /// Called during SetupPreliminaryConfiguration, merged with preliminary sources.
        /// @param [in] config The client config.
        /// @param [out] early_config The early configuration (populate global_params only; sources are for capture-time).
        virtual void GetEarlyConfiguration([[maybe_unused]] const ConfigType& config, [[maybe_unused]] UberTraceConfig& early_config)
        {
            // Default: no early configuration
        }

        /// @brief Generates an UberTrace config from the client config.
        /// @param [in] config The client config.
        /// @param [out] capture_config The generated UberTrace config.
        /// @return The result of generating the configuration.
        virtual Result GenerateCaptureConfig(const ConfigType& config, UbertraceCaptureConfig& capture_config) = 0;

        /// @brief Returns the auto capture config that should be used or empty if auto capture should be skipped.
        /// @return The capture config to use for auto capture or empty if no auto capture should be performed.
        virtual std::optional<UbertraceAutoCaptureConfig> GetAutoCaptureConfig([[maybe_unused]] const ConfigType& config)
        {
            return {};
        }

        /// @brief Called when tracing starts (before BeginTrace).
        virtual void TracingStarted()
        {
        }

        /// @brief Called when tracing ends.
        virtual void TracingEnded()
        {
        }

    public:
        Result RequestAbortTrace() override;
        bool   IsAbortTraceSupported() override;

        Result PrepareForDelayedCapture() override;
        Result RequestBeginTrace(uint32_t capture_mode) override;

        ClientState GetState() override;

    private:
        /// @brief Requests that a trace be taken.
        /// @param [in] capture_mode The meaning of this capture mode is up to interpretation of the implementing client, but 0 (default) should always be valid.
        /// @param [in] capture_type trueThe type of capture being executed.
        /// @return kSuccess if the trace was successfully requested.
        Result RequestBeginTrace(uint32_t capture_mode, UberTraceCaptureType capture_type);

    public:
        /// @brief Sets whether tracing is disabled or not.
        /// @param [in] disabled true if tracing should be disabled, false otherwise.
        void SetIsDisabled(bool disabled) const;

    protected:
        /// @brief Gets the connection info for the client.
        /// @return The connection info for the client.
        [[nodiscard]] const ClientConnection& GetConnInfo() const;

        /// @brief Gets an object that can do logging.
        /// @return The logger object.
        [[nodiscard]] const std::shared_ptr<Logger>& GetLogger() const;

        /// @brief Gets the UberTrace features.
        /// @return The Ubertrace features.
        [[nodiscard]] const UbertraceFeatures& GetFeatures() const;

    private:
        ClientConnection                       conn_info_{};                                           ///< The connection info for the client.
        ClientUtils<ConfigType>&               client_utils_;                                          ///< The utils that the client can use.
        std::unique_ptr<UbertraceUser>         user_;                                                  ///< The UberTrace user to use.
        std::shared_ptr<AdditionalChunkWriter> additional_chunk_writer_;                               ///< Object used for system info writing.
        ChunkWriterReservationId               chunk_reservation_ = kInvalidChunkWriterReservationId;  ///< Reservation for chunk writer.
        ProcessInfoChunk                       process_info_chunk_{};                                  ///< The process info chunk.
        CancellableTimer                       timer_;                                                 ///< Timer for compute auto capture.
        bool                                   enable_tracing_ = false;  ///< true if this client should enable tracing, false otherwise.
    };

}  // namespace devtrace

#include "ubertrace_client.inl"

#endif
