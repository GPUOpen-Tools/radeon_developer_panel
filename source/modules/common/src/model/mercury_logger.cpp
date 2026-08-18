// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for DevTrace logging interface implementation.

#include "model/mercury_logger.h"

#include "api/dev_tools_logging.h"

MercuryLogger::MercuryLogger(struct DevToolsLoggingApi* logging_api)
    : MercuryLogger(logging_api, "Default")
{
}

MercuryLogger::MercuryLogger(DevToolsLoggingApi* logging_api, const std::string& source)
    : logging_api_(logging_api)
    , source_(source)
{
}

std::shared_ptr<devtrace::Logger> MercuryLogger::WithSource(const std::string& source)
{
    return std::shared_ptr<MercuryLogger>(new MercuryLogger(logging_api_, source));
}

void MercuryLogger::Verbose(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const
{
    logging_api_->verbose(source_, text, pid, umd_connection_id);
}

void MercuryLogger::Info(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const
{
    logging_api_->info(source_, text, pid, umd_connection_id);
}

void MercuryLogger::Warning(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const
{
    logging_api_->warning(source_, text, pid, umd_connection_id);
}

void MercuryLogger::Error(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const
{
    logging_api_->error(source_, text, pid, umd_connection_id);
}
