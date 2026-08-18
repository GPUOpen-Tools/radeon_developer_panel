// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for DevTrace logging interface implementation.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_MODEL_MERCURY_LOGGER_H_
#define RDP_SOURCE_MODULES_COMMON_INC_MODEL_MERCURY_LOGGER_H_

#include <string>

#include <MercuryModuleExt.h>
#include <ddApi.h>

#include <logging.h>

/// @brief Logger implementation that uses a mercury logging interface.
class MercuryLogger : public devtrace::Logger
{
public:
    /// @brief Constructor.
    /// @param logging_api The API used for logging.
    explicit MercuryLogger(struct DevToolsLoggingApi* logging_api);

private:
    /// @brief Constructor.
    /// @param logging_api The API used for logging.
    /// @param source Source for messages.
    MercuryLogger(struct DevToolsLoggingApi* logging_api, const std::string& source);

public:
    std::shared_ptr<Logger> WithSource(const std::string& source) override;

    void Verbose(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;
    void Info(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;
    void Warning(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;
    void Error(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;

private:
    struct DevToolsLoggingApi* logging_api_;  ///< The API used for logging.
    std::string                source_;       ///< Source for messages.
};

#endif
