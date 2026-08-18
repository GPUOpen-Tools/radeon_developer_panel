// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for profiling prelaunch settings helper.

#include "profiling_prelaunch_settings_helper.h"

ProfilingPrelaunchSettingsHelper::ProfilingPrelaunchSettingsHelper(const std::shared_ptr<devtrace::RgpTraceSource>& trace_source)
    : TraceSourcePrelaunchSettingsHelper(trace_source)
    , trace_source_(trace_source)
{
}

bool ProfilingPrelaunchSettingsHelper::SetShaderInstrumentationEnabled(bool enabled)
{
    return trace_source_->SetShaderInstrumentationEnabled(enabled) == devtrace::Result::kSuccess;
}
