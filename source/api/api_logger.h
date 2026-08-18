// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for devtrace API logger implementation.

#include <dipper.h>
#include <logging.h>

#include "RdpCaptureApi.h"

class ApiLogger : public devtrace::Logger
{
public:
    /// @brief Constructor.
    /// @param [in] callback The callback to notify when a message is logged.
    DIP(ApiLogger(const RdpCaptureLogCallback& callback));

private:
    /// @brief Constructor.
    /// @param [in] callback The callback to notify when a message is logged.
    /// @param [in] source The source to log out.
    ApiLogger(const RdpCaptureLogCallback& callback, const std::string& source);

public:
    std::shared_ptr<Logger> WithSource(const std::string& source) override;

protected:
    void Verbose(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;
    void Info(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;
    void Warning(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;
    void Error(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;

private:
    /// @brief Append message to log with given level.
    /// @param [in] level The log level of the message.
    /// @param [in] text The string message.
    /// @param [in] pid The PID that this logging message is associated with.
    /// @param [in] umd_connection_id The UMD connection id.
    void Log(RdpCaptureLogLevel level, const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const;

private:
    RdpCaptureLogCallback callback_{};  ///< The callback to notify when a message is logged.
    std::string           source_;      ///< The source to log out.
};
