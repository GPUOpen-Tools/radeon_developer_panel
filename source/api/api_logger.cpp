// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for devtrace API logger implementation.

#include "api_logger.h"

ApiLogger::ApiLogger(const RdpCaptureLogCallback& callback)
    : ApiLogger(callback, "Default")
{
}

ApiLogger::ApiLogger(const RdpCaptureLogCallback& callback, const std::string& source)
    : callback_(callback)
    , source_(source)
{
}

std::shared_ptr<devtrace::Logger> ApiLogger::WithSource(const std::string& source)
{
    return std::shared_ptr<ApiLogger>(new ApiLogger(callback_, source));
}

void ApiLogger::Verbose(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const
{
    Log(kRdpCaptureLogLevelVerbose, text, pid, umd_connection_id);
}

void ApiLogger::Info(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const
{
    Log(kRdpCaptureLogLevelInfo, text, pid, umd_connection_id);
}

void ApiLogger::Warning(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const
{
    Log(kRdpCaptureLogLevelWarning, text, pid, umd_connection_id);
}

void ApiLogger::Error(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const
{
    Log(kRdpCaptureLogLevelError, text, pid, umd_connection_id);
}

void ApiLogger::Log(RdpCaptureLogLevel level, const std::string& text, uint32_t pid, [[maybe_unused]] DDConnectionId umd_connection_id) const
{
    if (callback_.log != nullptr)
    {
        callback_.log(callback_.user_data, level, source_.c_str(), pid, text.c_str());
    }
}
