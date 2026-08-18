// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for trace source status.

#include "source_status.h"

namespace devtrace
{
    bool TraceSourceStatus::operator==(const TraceSourceStatus& other) const
    {
        bool are_connections_equal = current_connections.size() == other.current_connections.size();
        if (are_connections_equal)
        {
            for (const auto& pair : current_connections)
            {
                if (other.current_connections.count(pair.first) == 0 || other.current_connections.at(pair.first) != pair.second)
                {
                    are_connections_equal = false;
                    break;
                }
            }
        }

        return are_connections_equal && other.num_bytes_dumped == num_bytes_dumped && other.total_bytes_to_dump == total_bytes_to_dump &&
               other.trace_stage == trace_stage && other.stage_progress == stage_progress;
    }

    void TraceSourceStatus::ChangeStage(TraceSourceStage new_stage)
    {
        if (trace_stage != new_stage)
        {
            disabled_reason = new_stage == TraceSourceStage::kDisabled ? DisabledReason::kNoReason : DisabledReason::kEnabled;
            stage_progress  = 0.0;
        }

        if (new_stage != TraceSourceStage::kDumping)
        {
            num_bytes_dumped    = 0;
            total_bytes_to_dump = 0;
        }

        trace_stage = new_stage;
    }

}  // namespace devtrace
