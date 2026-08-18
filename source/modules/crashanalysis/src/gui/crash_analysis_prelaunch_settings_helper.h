// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for crash analysis prelaunch settings helper.

#ifndef RDP_SOURCE_MODULES_CRASH_ANALYSIS_SRC_GUI_CRASH_ANALYSIS_PRELAUNCH_SETTINGS_HELPER_H_
#define RDP_SOURCE_MODULES_CRASH_ANALYSIS_SRC_GUI_CRASH_ANALYSIS_PRELAUNCH_SETTINGS_HELPER_H_

#include <memory>

#include <rgd_trace_source.h>

#include <common/inc/model/prelaunch_settings_helper.h>

/// @brief Implementation of PrelaunchSettingsHelper for crash analysis.
class CrashAnalysisPrelaunchSettingsHelper : public TraceSourcePrelaunchSettingsHelper
{
public:
    /// @brief Constructor.
    /// @param [in] trace_source The trace source to use.
    explicit CrashAnalysisPrelaunchSettingsHelper(const std::shared_ptr<devtrace::RgdTraceSource>& trace_source);

private:
    std::shared_ptr<devtrace::RgdTraceSource> trace_source_;  ///< The trace source to use.
};

#endif
