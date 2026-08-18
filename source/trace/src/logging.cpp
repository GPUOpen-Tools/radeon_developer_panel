// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for DevTrace logging interface.

#include "logging.h"

#include <chrono>
#include <iostream>
#include <utility>

#include "dev_trace_common.h"

namespace devtrace
{
    void NoopLogger::Verbose([[maybe_unused]] const std::string&   text,
                             [[maybe_unused]] const uint32_t       pid,
                             [[maybe_unused]] const DDConnectionId umd_connection_id) const
    {
    }

    void NoopLogger::Info([[maybe_unused]] const std::string&   text,
                          [[maybe_unused]] const uint32_t       pid,
                          [[maybe_unused]] const DDConnectionId umd_connection_id) const
    {
    }

    void NoopLogger::Warning([[maybe_unused]] const std::string&   text,
                             [[maybe_unused]] const uint32_t       pid,
                             [[maybe_unused]] const DDConnectionId umd_connection_id) const
    {
    }

    void NoopLogger::Error([[maybe_unused]] const std::string&   text,
                           [[maybe_unused]] const uint32_t       pid,
                           [[maybe_unused]] const DDConnectionId umd_connection_id) const
    {
    }

    std::shared_ptr<Logger> NoopLogger::WithSource([[maybe_unused]] const std::string& source)
    {
        return std::make_shared<NoopLogger>();
    }

    StdIoLogger::StdIoLogger()
        : StdIoLogger(false)
    {
    }

    StdIoLogger::StdIoLogger(const bool use_std_err)
        : StdIoLogger("Default", use_std_err)
    {
    }

    StdIoLogger::StdIoLogger(std::string source, const bool use_std_err)
        : source_(std::move(source))
        , use_std_err_(use_std_err)
    {
    }

    std::shared_ptr<Logger> StdIoLogger::WithSource(const std::string& source)
    {
        return std::shared_ptr<StdIoLogger>(new StdIoLogger(source, use_std_err_));
    }

    void StdIoLogger::Verbose(const std::string& text, const uint32_t pid, const DDConnectionId umd_connection_id) const
    {
        std::cout << FormatMessage("VERBOSE", text, pid, umd_connection_id) << '\n';
    }

    void StdIoLogger::Info(const std::string& text, const uint32_t pid, const DDConnectionId umd_connection_id) const
    {
        std::cout << FormatMessage("INFO", text, pid, umd_connection_id) << '\n';
    }

    void StdIoLogger::Warning(const std::string& text, const uint32_t pid, const DDConnectionId umd_connection_id) const
    {
        std::cout << FormatMessage("WARNING", text, pid, umd_connection_id) << '\n';
    }

    void StdIoLogger::Error(const std::string& text, const uint32_t pid, const DDConnectionId umd_connection_id) const
    {
        (use_std_err_ ? std::cerr : std::cout) << FormatMessage("ERROR", text, pid, umd_connection_id) << '\n';
    }

    std::string StdIoLogger::FormatMessage(const std::string&                    level_str,
                                           const std::string&                    text,
                                           const uint32_t                        pid,
                                           [[maybe_unused]] const DDConnectionId umd_connection_id) const
    {
        if (pid == 0)
        {
            return fmt::format("({}) {} [{}] {}", GetTimeAsString(), level_str, source_, text);
        }

        return fmt::format("({}) {} [{} - PID: {}] {}", GetTimeAsString(), level_str, source_, pid, text);
    }

};  // namespace devtrace
