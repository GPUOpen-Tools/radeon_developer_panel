// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for base interface for prelaunch settings helper.

#include "model/prelaunch_settings_helper.h"

TraceSourcePrelaunchSettingsHelper::TraceSourcePrelaunchSettingsHelper(const std::shared_ptr<devtrace::TraceSource>& trace_source)
    : trace_source_(trace_source)
{
}
