// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the continuous trace source base.

#ifndef RDP_SOURCE_TRACE_INC_CONTINUOUS_TRACE_SOURCE_H_
#define RDP_SOURCE_TRACE_INC_CONTINUOUS_TRACE_SOURCE_H_

#include <cstdint>
#include <string>

#include <dd_common_api.h>

#include "trace_source.h"

namespace devtrace
{
    /// @brief A trace source that is continuously pulling data.
    class ContinuousTraceSource : public TraceSource
    {
    public:
        /// @brief Destructor.
        ~ContinuousTraceSource() override = default;

        /// @brief Requests that the trace be dumped to disk.
        ///
        /// This will end tracing.
        /// @param [in] connection_id The identifier of the connection to dump.
        /// @return kSuccess if the dump was successful.
        virtual Result RequestDump(DDConnectionId connection_id) = 0;

        /// @brief Adds a marker with the given name to the trace in progress.
        ///
        /// If there is not currently a trace in progress, this will return kError.
        /// @param [in] connection_id The identifier of the connection to add a marker for.
        /// @param [in] marker The name of the marker to add to the trace. This should be formatted as UTF-8.
        /// @return kSuccess if the marker was successfully added to the trace.
        virtual Result AddMarker(DDConnectionId connection_id, const std::string& marker) = 0;
    };

};  // namespace devtrace

#endif
