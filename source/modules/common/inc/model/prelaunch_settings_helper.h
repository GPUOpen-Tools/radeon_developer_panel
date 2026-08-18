// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for base interface for prelaunch settings helper.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_MODEL_PRELAUNCH_SETTINGS_HELPER_H_
#define RDP_SOURCE_MODULES_COMMON_INC_MODEL_PRELAUNCH_SETTINGS_HELPER_H_

#include <memory>

#include <trace_source.h>

/// @brief Interface for an object that helps handle additional logic pre-launch settings beyond saving them in the userdata.
///
/// Note: The prelaunch settings editable state is now determined by the view layer by checking
/// if a process is connected (status.pid == 0) and communicated via Qt signals/slots.
class PrelaunchSettingsHelper
{
public:
    virtual ~PrelaunchSettingsHelper() = default;
};

/// @brief PrelaunchSettingsHelper that will say that prelaunch settings are always enabled.
class AlwaysPrelaunchSettingsHelper : public PrelaunchSettingsHelper
{
public:
    ~AlwaysPrelaunchSettingsHelper() override = default;
};

/// @brief Implementation of PrelaunchSettingsHelper that uses a trace source to get whether or not the prelaunch settings should be enabled.
class TraceSourcePrelaunchSettingsHelper : public PrelaunchSettingsHelper
{
public:
    /// @brief Constructor.
    /// @param [in] trace_source The trace source to use.
    TraceSourcePrelaunchSettingsHelper(const std::shared_ptr<devtrace::TraceSource>& trace_source);

    ~TraceSourcePrelaunchSettingsHelper() override = default;

private:
    std::shared_ptr<devtrace::TraceSource> trace_source_;  ///< The trace source to use.
};

#endif
