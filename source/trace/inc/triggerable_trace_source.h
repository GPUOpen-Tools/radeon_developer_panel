// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for a trace source that can be triggered at any time.

#ifndef RDP_SOURCE_TRACE_INC_TRIGGERABLE_TRACE_SOURCE_H_
#define RDP_SOURCE_TRACE_INC_TRIGGERABLE_TRACE_SOURCE_H_

#include <cstdint>
#include <string>

#include <dd_common_api.h>

#include "trace_source.h"

namespace devtrace
{
    /// @brief A trace source that can be triggered at any time.
    class TriggerableTraceSource : public TraceSource
    {
    public:
        /// @brief Destructor.
        ~TriggerableTraceSource() override = default;

        /// @brief This can be called to move the connection into the waiting for capture state.
        ///
        /// After the trace source is in this state, calling RequestBeginTrace will actually begin a capture.
        /// @param [in] connection_id The identifier of the connection to capture.
        /// @return kSuccess if the connection was successfully prepared for capture.
        virtual Result PrepareForDelayedCapture(DDConnectionId connection_id) = 0;

        /// @brief Requests that a trace be taken.
        /// @param [in] connection_id The identifier of the connection to capture.
        /// @param [in] capture_mode The meaning of this capture mode is up to interpretation of the implementing trace source, but 0 (default) should always be valid.
        /// @return kSuccess if the trace was successfully requested.
        virtual Result RequestBeginTrace(DDConnectionId connection_id, uint32_t capture_mode = 0) = 0;

        /// @brief Gets the supported capture modes for the specific connection.
        /// @param [in] connection_id The identifier of the connection to get the supported modes for.
        /// @param [out] out_modes The capture modes for the client.
        virtual void GetSupportedCaptureModes(DDConnectionId connection_id, std::vector<uint32_t>& out_modes) = 0;
    };

};  // namespace devtrace

#endif
