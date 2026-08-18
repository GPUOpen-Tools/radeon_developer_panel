// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  The definition for the shared DevTools logging API and interfaces.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_API_DEV_TOOLS_LOGGING_H_
#define RDP_SOURCE_MODULES_COMMON_INC_API_DEV_TOOLS_LOGGING_H_

#include <cstdint>
#include <string>

#include <dd_common_api.h>
#include "logging_definitions.h"

static constexpr const char* kDevToolsLoggingApiName         = "DevToolsLoggingApi";  ///< The name of the DevTools logging api.
static constexpr uint32_t    kDevToolsLoggingApiVersionMajor = 1;                     ///< The major version of the DevTools logging api.
static constexpr uint32_t    kDevToolsLoggingApiVersionMinor = 0;                     ///< The minor version of the DevTools logging api.
static constexpr uint32_t    kDevToolsLoggingApiVersionPatch = 0;                     ///< The patch of the DevTools logging api.

/// @brief The API for DevTools logging API.
struct DevToolsLoggingApi
{
    /// @brief Append message to log with info log level.
    /// @param [in] source The source of this logging message or empty string if it is a generic logging message.
    /// @param [in] text The string message.
    /// @param [in] pid The PID that this logging message is associated with or kLoggingInvalidPid if it is not associated with a process.
    /// @param [in] umd_connection_id The UMD connection id or kLoggingInvalidUmdId if not applicable.
    void (*info)(const std::string& source, const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) = nullptr;

    /// @brief Append message to log with warning log level.
    /// @param [in] source The source of this logging message or empty string if it is a generic logging message.
    /// @param [in] text The string message.
    /// @param [in] pid The PID that this logging message is associated with or kLoggingInvalidPid if it is not associated with a process.
    /// @param [in] umd_connection_id The UMD connection id or kLoggingInvalidUmdId if not applicable.
    void (*warning)(const std::string& source, const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) = nullptr;

    /// @brief Append message to log with error log level.
    /// @param [in] source The source of this logging message or empty string if it is a generic logging message.
    /// @param [in] text The string message.
    /// @param [in] pid The PID that this logging message is associated with or kLoggingInvalidPid if it is not associated with a process.
    /// @param [in] umd_connection_id The UMD connection id or kLoggingInvalidUmdId if not applicable.
    void (*error)(const std::string& source, const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) = nullptr;

    /// @brief Append message to log with verbose log level.
    /// @param [in] source The source of this logging message or empty string if it is a generic logging message.
    /// @param [in] text The string message.
    /// @param [in] pid The PID that this logging message is associated with or kLoggingInvalidPid if it is not associated with a process.
    /// @param [in] umd_connection_id The UMD connection id or kLoggingInvalidUmdId if not applicable.
    void (*verbose)(const std::string& source, const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) = nullptr;
};

#endif
