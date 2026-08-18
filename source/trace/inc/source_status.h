// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for trace source status.

#ifndef RDP_SOURCE_TRACE_INC_SOURCE_STATUS_H_
#define RDP_SOURCE_TRACE_INC_SOURCE_STATUS_H_

#include "dev_trace_common.h"

#include <cstdint>
#include <unordered_map>

#if __cplusplus >= 202302L
#include <stacktrace>
#endif

namespace devtrace
{
    /// @brief The status of a trace source.
    struct TraceSourceStatus
    {
        /// @brief The number of bytes that have been dumped.
        ///
        /// This value will be 0 until the trace source enters the dumping state.
        /// When it hits a terminal state (error, done, idle) this value will be reset to 0.
        uint64_t num_bytes_dumped = 0;

        /// @brief The number of bytes that will be dumped.
        ///
        /// This value will be 0 until the trace source enters the dumping state.
        /// When it hits a terminal state (error, done, idle) this value will be reset to 0.
        uint64_t total_bytes_to_dump = 0;

        /// @brief The process id that the trace source is connected to.
        uint32_t pid = 0;

        /// @brief The name of the application that the trace source is connected to.
        std::string application_name;

        /// @brief A list of the current connections that the trace source is connected to.
        ///
        /// The keys are unqiue identifiers of the connections, the values are the APIs being used by those connections.
        std::unordered_map<uint16_t, Api> current_connections;

        /// @brief A number [0.0, 1.0] that represents the current progress of the current stage.
        ///
        /// This is unused for most stages. If it's unused, the value will be 0.0.
        float stage_progress = 0.0;

        /// @brief Equality operator for another trace source status.
        /// @param [in] other The other trace status to compare this trace status to.
        /// @return true if this trace source and the other trace source are the same, false otherwise.
        bool operator==(const TraceSourceStatus& other) const;

        /// @brief true if a call to abort trace is supported.
        ///
        /// This value will be true if abort trace is currently a no-op since there is nothing to cancel.
        bool abort_trace_supported = false;

        /// @brief true if a call to abort processing is supported.
        ///
        /// This value will be true if abort processing is currently a no-op since there is nothing to cancel.
        bool abort_processing_supported = false;

        /// @brief Changes the stage.
        /// @param [in] new_stage The new stage.
        void ChangeStage(TraceSourceStage new_stage);

        /// @brief Gets the current trace source stage.
        /// @return The trace source stage.
        TraceSourceStage GetStage() const
        {
            return trace_stage;
        }

        /// @brief If the trace source stage is disabled, this should contain the reason it was disabled.
        ///
        /// When ChangeStage is called, this will automatically be set to UnsupportedReason::kNoReason if the state is disabled, but it is up to the owning
        /// trace source to fill in the exact reason. When ChangeStage is called with anything but TraceSourceStage::kDisabled, this will be reset to
        /// DisabledReason::kEnabled.
        DisabledReason disabled_reason = DisabledReason::kEnabled;

    private:
        /// @brief The FSM state of the trace source.
        TraceSourceStage trace_stage = TraceSourceStage::kDisconnected;
    };

    struct TraceSourceStatusEventArgs
    {
        TraceSourceStatus new_status;  ///< The new status update.
        TraceSourceStatus old_status;  ///< The old status update.
#if __cplusplus >= 202302L
        std::stacktrace stack;  ///< The stack trace at the time of the status update.
#endif
    };

    struct TraceSourceStatusEvent
    {
        void*                                                         listener;  ///< The listener object.
        std::function<void(void*, const TraceSourceStatusEventArgs&)> callback;  ///< The callback function.
    };

    struct TraceCaptureProgress
    {
        TraceSourceStatus status;  ///< Current trace source status.

        /// @brief The number of bytes that have been dumped.
        ///
        /// This value will be 0 until the trace source enters the dumping state.
        /// When it hits a terminal state (error, done, idle) this value will be reset to 0.
        uint64_t num_bytes_dumped = 0;

        /// @brief The number of bytes that will be dumped.
        ///
        /// This value will be 0 until the trace source enters the dumping state.
        /// When it hits a terminal state (error, done, idle) this value will be reset to 0.
        uint64_t total_bytes_to_dump = 0;

        /// A number [0.0, 1.0] that represents the current progress of the capture.
        float progress = 0.0f;

        std::string progress_text;
    };

    struct TraceCaptureProgressEventArgs
    {
        TraceCaptureProgress new_progress;  ///< The new progress update.
        TraceCaptureProgress old_progress;  ///< The old progress update.
    };

    struct TraceCaptureProgressEvent
    {
        void*                                                            listener;  ///< The listener object.
        std::function<void(void*, const TraceCaptureProgressEventArgs&)> callback;  ///< The callback function.
    };

    struct TraceCompletionEventArgs
    {
        TraceCompletionResult result;  ///< Trace completion result
    };

    struct TraceCompletionEvent
    {
        void*                                                       listener;  ///< The listener object.
        std::function<void(void*, const TraceCompletionEventArgs&)> callback;  ///< The callback function.
    };

    /// @brief Provides the user-readable string describing the trace source stage.
    /// @param [in] stage The stage to describe.
    /// @return A string describing the stage.
    inline std::string TraceSourceStageToString(TraceSourceStage stage)
    {
        switch (stage)
        {
        case devtrace::TraceSourceStage::kDisconnected:
            return "Offline";
        case devtrace::TraceSourceStage::kIdle:
            return "Ready";
        case devtrace::TraceSourceStage::kWaitingToBeginCapture:
            return "Waiting to begin capture";
        case devtrace::TraceSourceStage::kCapturing:
            return "Capturing";
        case devtrace::TraceSourceStage::kDumping:
            return "Dumping";
        case devtrace::TraceSourceStage::kProcessing:
            return "Processing";
        case devtrace::TraceSourceStage::kDone:
            return "Done";
        case devtrace::TraceSourceStage::kDisabled:
            return "Unsupported";
        case devtrace::TraceSourceStage::kError:
            return "Error";
        case devtrace::TraceSourceStage::kBusy:
            return "Busy";
        default:
            return "Unknown";
        };
    }

}  // namespace devtrace

#endif
