// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for base RMV trace source.

#ifndef RDP_SOURCE_TRACE_INC_RMV_TRACE_SOURCE
#define RDP_SOURCE_TRACE_INC_RMV_TRACE_SOURCE

#include <string>

#include "configurable.h"
#include "continuous_trace_source.h"

namespace devtrace
{
    /// @brief Configuration for RMV trace source.
    ///
    /// Currently there are no configuration options for RMV, but this exists just in case
    /// in the future there are.
    struct RmvTraceSourceConfig
    {
    };

    /// @brief Abstract RMV trace source.
    class RmvTraceSource : public ContinuousTraceSource, public Configurable<RmvTraceSourceConfig>
    {
    public:
        /// @brief Destructor.
        ~RmvTraceSource() override = default;
    };

};  // namespace devtrace

#endif
