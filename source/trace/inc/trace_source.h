// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for generic trace source.

#ifndef RDP_SOURCE_TRACE_INC_TRACE_SOURCE_H_
#define RDP_SOURCE_TRACE_INC_TRACE_SOURCE_H_

#include "client_connections.h"
#include "dev_trace_common.h"
#include "source_status.h"

namespace devtrace
{
    /// @brief Abstract trace source.
    class TraceSource : public ClientConnectionSubscriberV2
    {
    public:
        /// @brief Destructor.
        ~TraceSource() override = default;

        /// @brief Requests that an in-progress trace be aborted.
        ///
        /// It's possible that a successful request won't actually do anything because it is too late in the trace.
        /// @param [in] umd_connection_id The identifier of the connection to abort the trace for.
        /// @return kSuccess if an abort was successfully requested.
        virtual Result RequestAbortTrace(DDConnectionId umd_connection_id) = 0;

        /// @brief Requests that an in-progress trace be aborted.
        ///
        /// It's possible that a successful request won't actually do anything because it is too late in the trace.
        /// @return kSuccess if an abort was successfully requested.
        virtual Result RequestAbortProcessing() = 0;

        /// @brief Registers a status event callback with trace source
        /// @param [in] event The even callback structure
        virtual void RegisterStatusEvent(const TraceSourceStatusEvent& event) = 0;

        /// @brief Registers a trace completion event callback with trace source
        /// @param [in] event The even callback structure
        virtual void RegisterTraceCompletionEvent(const TraceCompletionEvent& event) = 0;

        /// @brief Registers a trace capture progress event callback with trace source
        /// @param [in] event The even callback structure
        virtual void RegisterTraceCaptureProgressEvent(const TraceCaptureProgressEvent& event) = 0;

        /// @brief Force a broadcast of current trace source status to all listeners.
        virtual void QueryStatus() = 0;
    };

}  // namespace devtrace

#endif
