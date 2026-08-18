// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for trace source client.

#ifndef RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_TRACE_SOURCE_CLIENT_H_
#define RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_TRACE_SOURCE_CLIENT_H_

#include <memory>
#include <mutex>
#include <vector>

#if __cplusplus >= 202302L
#include <stacktrace>
#endif

#include <ddApi.h>

#include "client_connections.h"
#include "dev_trace_common.h"
#include "file_writing.h"
#include "logging.h"
#include "overlay_manager.h"

namespace devtrace
{
    /// @brief The states for a single trace source client.
    ///
    /// These states are ordered such that taking the max of the states of several clients will yield the stage of the trace source overall.
    enum class ClientState : uint8_t
    {
        kDisabled = 0,
        kError,
        kDone,
        kBusy,
        kIdle,
        kWaitingToBeginCapture,
        kCapturing,
        kPostProcessing,
        kDumping
    };

    struct PostProcessingEventArgs
    {
        float       progress = 0.0f;
        std::string progress_text;
    };

    struct PostProcessingEvent
    {
        void*                                                      listener;  ///< Listener for event.
        std::function<void(void*, const PostProcessingEventArgs&)> callback;  ///< Event callback.
    };

    struct ClientStateChangeEventArgs
    {
        ClientState new_state;  ///< New client state.
        ClientState old_state;  ///< Old client state.
#if __cplusplus >= 202302L
        std::stacktrace stack;  ///< Stacktrace of where the state change was triggered.
#endif
    };

    struct ClientStateChangeEvent
    {
        void*                                                         listener;  ///< Listener for event.
        std::function<void(void*, const ClientStateChangeEventArgs&)> callback;  ///< Event callback.
    };

    /// @brief Event structure for no crash detected callback.
    struct NoCrashDetectedEvent
    {
        void*                      listener = nullptr;  ///< Listener for event.
        std::function<void(void*)> callback;            ///< Event callback.
    };

    /// @brief Returns whether the client state is considered capturing.
    /// @param [in] state The state to return whether it is capturing.
    /// @return true if the state is capturing, false otherwise.
    inline bool IsCapturing(const ClientState state)
    {
        switch (state)
        {
        case ClientState::kWaitingToBeginCapture:
        case ClientState::kCapturing:
        case ClientState::kDumping:
        case ClientState::kPostProcessing:
            return true;
        default:
            break;
        }

        return false;
    }

    /// @brief Converts a client state to an overlay state.
    /// @param [in] state The state to convert.
    /// @return The converted state.
    inline OverlayState ClientStateToOverlayState(const ClientState state)
    {
        switch (state)
        {
        case ClientState::kDisabled:
            return OverlayState::kDisabled;
        case ClientState::kError:
            return OverlayState::kError;
        case ClientState::kDone:
            return OverlayState::kDone;
        case ClientState::kBusy:
            return OverlayState::kBusy;
        case ClientState::kIdle:
            return OverlayState::kIdle;
        case ClientState::kWaitingToBeginCapture:
            return OverlayState::kWaitingToBeginCapture;
        case ClientState::kCapturing:
            return OverlayState::kCapturing;
        case ClientState::kPostProcessing:
            return OverlayState::kProcessing;
        case ClientState::kDumping:
            return OverlayState::kDumping;
        default:
            return OverlayState::kInactive;
        }
    }

    /// @brief Converts a client state to a trace source stage.
    /// @param [in] state The state to convert.
    /// @return The converted state.
    inline TraceSourceStage ClientStateToTraceSourceStage(const ClientState state)
    {
        switch (state)
        {
        case ClientState::kIdle:
            return TraceSourceStage::kIdle;
        case ClientState::kDisabled:
            return TraceSourceStage::kDisabled;
        case ClientState::kError:
            return TraceSourceStage::kError;
        case ClientState::kDone:
            return TraceSourceStage::kDone;
        case ClientState::kBusy:
            return TraceSourceStage::kBusy;
        case ClientState::kWaitingToBeginCapture:
            return TraceSourceStage::kWaitingToBeginCapture;
        case ClientState::kCapturing:
            return TraceSourceStage::kCapturing;
        case ClientState::kPostProcessing:
            return TraceSourceStage::kProcessing;
        case ClientState::kDumping:
            return TraceSourceStage::kDumping;
        default:
            return TraceSourceStage::kDisconnected;
        }
    }

    /// @brief Utilities to be used for a trace source client.
    struct BaseClientUtils
    {
        virtual ~BaseClientUtils() = default;
        /// @brief Should be called when a trace is completed.
        /// @param [in] status The status of the trace.
        /// @param [in] path The path of the trace.
        /// @param [in] umd_connection_id The UMD connection that created this trace.
        virtual void TraceCompleted(TraceCompletionStatus status, const std::string& path, DDConnectionId umd_connection_id) = 0;

        /// @brief Reports capture progress.
        /// @param [in] progress The progress information to report.
        virtual void ReportCaptureProgress(const WritingProgress& progress) = 0;

        /// @brief Gets a byte writer.
        /// @param [out] path The path that the byte writer will write to.
        /// @return A byte writer.
        virtual std::unique_ptr<ByteWriter> GetByteWriter(std::string& path) = 0;

        /// @brief Gets a rdf file writer.
        /// @param [out] path The path that the byte writer will write to.
        /// @return A rdf file writer.
        virtual std::unique_ptr<RdfWriter> GetRdfWriter(std::string& path) = 0;

        /// @brief Gets an object that can do logging.
        /// @return The logger object.
        virtual const std::shared_ptr<Logger>& GetLogger() = 0;
    };

    /// @brief Utilities to be used for a trace source client.
    /// @param [in] config The configuration for the client.
    template <typename ConfigType>
    struct ClientUtils : BaseClientUtils
    {
        /// @brief Gets the client config.
        /// @return The client config.
        virtual const ConfigType& GetClientConfig() = 0;

        virtual void EmitPostProcessEvent(const std::string& progress_text, float progress) = 0;
    };

    /// @brief A client that maps 1:1 with a driver connection.
    /// @tparam ConfigType The configuration type for the client.
    template <typename ConfigType>
    class Client
    {
    public:
        virtual ~Client()      = default;
        using ClientConfigType = ConfigType;  ///< Public facing declaration for the config type.

        /// @brief Gets the state that this client should be initialized on.
        /// @return The state that this client should be initialized on.
        [[nodiscard]] virtual DD_DRIVER_STATE GetInitDriverState() = 0;

        /// @brief Initializes the client.
        /// @return The result of the initialization.
        [[nodiscard]] virtual Result Initialize() = 0;

        /// @brief Disconnects the client.
        /// This method will complete the client disconnection logic.
        virtual void Disconnect() = 0;

        /// @brief Handles the connection reaching the driver state.
        /// @param [in] state The state to handle.
        /// @return The result of handling the state.
        [[nodiscard]] virtual Result HandleDriverState(DD_DRIVER_STATE state) = 0;

        /// @brief Requests that the current trace be aborted.
        /// @return The result of requesting the abortion.
        [[nodiscard]] virtual Result RequestAbortTrace() = 0;

        /// @brief Returns true if abort is supported, false otherwise.
        /// @return true if abort is supported, false otherwise.
        [[nodiscard]] virtual bool IsAbortTraceSupported() = 0;

        /// @brief Gets the current state of the client.
        /// @return The current state of the client.
        [[nodiscard]] virtual ClientState GetState()
        {
            return state_;
        }

        void RegisterClientPostProcessingRequired(const PostProcessingEvent& event)
        {
            std::scoped_lock lock(post_processing_required_events_mutex_);
            post_processing_events_.emplace_back(event);
        }

        void PostClientPostProcessingRequiredEvent(const PostProcessingEventArgs& args)
        {
            std::scoped_lock lock(post_processing_required_events_mutex_);
            for (auto& [listener, callback] : post_processing_events_)
            {
                if (callback)
                {
                    callback(listener, args);
                }
            }
        }

        /// @brief Registers a listener for client state change events.
        /// @param [in] event The event structure.
        void RegisterClientStateChangeEvent(const ClientStateChangeEvent& event)
        {
            std::scoped_lock lock(state_change_events_mutex_);
            state_change_events_.emplace_back(event);
        }

        /// @brief Registers a listener for no crash detected events.
        /// @param [in] event The event structure.
        void RegisterNoCrashDetectedEvent(const NoCrashDetectedEvent& event)
        {
            std::scoped_lock lock(no_crash_detected_event_mutex_);
            no_crash_detected_event_ = event;
        }

        void SetState(const ClientState new_state)
        {
            const std::scoped_lock lock(state_mutex_);
            const auto             current_state = state_;
            state_                               = new_state;

            ClientStateChangeEventArgs args{};
            args.new_state = state_;
            args.old_state = current_state;
#if __cplusplus >= 202302L
            args.stack = std::stacktrace::current();
#endif

            PostClientStateChangeEvent(args);
        }

        /// @brief Posts a client state change event for args.
        /// @param [in] args The arguments for the event.
        void PostClientStateChangeEvent(const ClientStateChangeEventArgs& args)
        {
            std::scoped_lock lock(state_change_events_mutex_);
            for (auto& [listener, callback] : state_change_events_)
            {
                if (callback)
                {
                    callback(listener, args);
                }
            }
        }

        /// @brief Posts the no crash detected event.
        void PostNoCrashDetectedEvent()
        {
            std::scoped_lock lock(no_crash_detected_event_mutex_);
            if (no_crash_detected_event_.listener != nullptr && no_crash_detected_event_.callback)
            {
                no_crash_detected_event_.callback(no_crash_detected_event_.listener);
            }
        }

    protected:
        std::mutex  state_mutex_;                 ///< Mutex guarding client state.
        ClientState state_ = ClientState::kIdle;  ///< The client state;

    private:
        std::mutex                       post_processing_required_events_mutex_;
        std::vector<PostProcessingEvent> post_processing_events_;

        std::mutex                          state_change_events_mutex_;  ///< Mutex guarding state change event callbacks.
        std::vector<ClientStateChangeEvent> state_change_events_;        ///< List of registered callbacks for state change event.

        std::mutex           no_crash_detected_event_mutex_;  ///< Mutex guarding no crash detected event callback.
        NoCrashDetectedEvent no_crash_detected_event_;        ///< Callback for no crash detected event.
    };

    /// @brief A client that can be used by a continuous trace source.
    /// @tparam ConfigType
    template <typename ConfigType>
    class ContinuousClient : public Client<ConfigType>
    {
    public:
        /// @brief Requests that the client dump a trace.
        /// @return The result of dumping the trace.
        [[nodiscard]] virtual Result RequestDump() = 0;

        /// @brief Adds a marker to the current trace.
        /// @param [in] marker The marker to add.
        /// @return The result of adding the marker.
        [[nodiscard]] virtual Result AddMarker(const std::string& marker) = 0;
    };

    /// @brief A client that can be used by a triggerable trace source.
    /// @tparam ConfigType
    template <typename ConfigType>
    class TriggerableClient : public Client<ConfigType>
    {
    public:
        /// @brief This can be called to move the client into the waiting for capture state.
        ///
        /// After the client is in this state, calling RequestBeginTrace will actually begin a capture.
        /// @return kSuccess if the trace source was successfully prepared for capture.
        [[nodiscard]] virtual Result PrepareForDelayedCapture() = 0;

        /// @brief Requests that a trace be taken.
        /// @param [in] capture_mode The meaning of this capture mode is up to interpretation of the implementing client, but 0 (default) should always be valid.
        /// @return kSuccess if the trace was successfully requested.
        [[nodiscard]] virtual Result RequestBeginTrace(uint32_t capture_mode) = 0;

        /// @brief Returns if the capture mode is supported.
        /// @param [in] mode The capture modes to check for support.
        /// @return true if the mode is supported, false otherwise.
        [[nodiscard]] virtual bool SupportsCaptureMode(uint32_t mode) const = 0;
    };

    template <typename ClientType>
    concept IsClient = std::is_base_of_v<Client<typename ClientType::ClientConfigType>, ClientType>;

    /// @brief Factory type for creating clients.
    /// @tparam ClientType The type of client to create.
    template <IsClient ClientType>
    class ClientFactory
    {
    public:
        virtual ~ClientFactory() = default;
        using UtilsType          = ClientUtils<typename ClientType::ClientConfigType>;  ///< Public facing declaration for ClientUtils type.

        /// @brief Creates a new client.
        /// @param [in] info The connection information for the client.
        /// @param [in] client_utils The utils that the client can use.
        /// @return The new client.
        [[nodiscard]] virtual std::unique_ptr<ClientType> CreateClient(const ClientConnection& info, UtilsType& client_utils) = 0;
    };

}  // namespace devtrace

#endif
