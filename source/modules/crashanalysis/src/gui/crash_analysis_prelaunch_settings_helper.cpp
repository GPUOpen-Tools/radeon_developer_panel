// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for crash analysis prelaunch settings helper.

#include "crash_analysis_prelaunch_settings_helper.h"

CrashAnalysisPrelaunchSettingsHelper::CrashAnalysisPrelaunchSettingsHelper(const std::shared_ptr<devtrace::RgdTraceSource>& trace_source)
    : TraceSourcePrelaunchSettingsHelper(trace_source)
    , trace_source_(trace_source)
{
}
