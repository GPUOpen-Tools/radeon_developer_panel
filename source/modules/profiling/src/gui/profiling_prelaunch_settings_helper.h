// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for profiling prelaunch settings helper.

#ifndef RDP_SOURCE_MODULES_PROFILING_SRC_GUI_PROFILING_PRELAUNCH_SETTINGS_HELPER_H_
#define RDP_SOURCE_MODULES_PROFILING_SRC_GUI_PROFILING_PRELAUNCH_SETTINGS_HELPER_H_

#include <memory>

#include <rgp_trace_source.h>

#include <common/inc/model/prelaunch_settings_helper.h>

/// @brief Implementation of PrelaunchSettingsHelper for profiling.
class ProfilingPrelaunchSettingsHelper : public TraceSourcePrelaunchSettingsHelper
{
public:
    /// @brief Constructor.
    /// @param [in] trace_source The trace source to use.
    ProfilingPrelaunchSettingsHelper(const std::shared_ptr<devtrace::RgpTraceSource>& trace_source);

    /// @brief Sets whether or not shader instrumentation is enabled.
    /// @param [in] enabled true if shader instrumentation should be enabled, false otherwise.
    /// @return true if setting shader instrumentation was successful, false otherwise.
    bool SetShaderInstrumentationEnabled(bool enabled);

private:
    std::shared_ptr<devtrace::RgpTraceSource> trace_source_;  ///< The trace source to use.
};

#endif
