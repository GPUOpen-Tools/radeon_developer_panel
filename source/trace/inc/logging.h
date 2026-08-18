// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for DevTrace logging interface.

#ifndef RDP_SOURCE_TRACE_INC_LOGGING_H_
#define RDP_SOURCE_TRACE_INC_LOGGING_H_

#include <cstdint>
#include <string>

#include <ddApi.h>

#define FMT_HEADER_ONLY
#include <fmt/chrono.h>
#include <fmt/format.h>

#include "dipper.h"
#include "logging_definitions.h"

namespace devtrace
{
    /// @brief Base class for a logger in DevTrace.
    class Logger
    {
    public:
        /// @brief Destructor.
        virtual ~Logger() = default;

        /// @brief Returns a new logger that will have the given source.
        /// @param [in] source The source of the new logger.
        /// @return A new logger with the given source.
        virtual std::shared_ptr<Logger> WithSource(const std::string& source) = 0;

        /// @brief Logs formatted error message.
        /// @param [in] fmt The format string.
        /// @param [in] pid The process id.
        /// @param [in] umd_connection_id The umd connection id.
        /// @param [in] args The variable arguments to print in formatted message.
        /// @code{.cpp}
        /// logger->LogError("Example print: {}", 0, "My message");
        /// @endcode
        template <typename... Args>
        void LogError(std::string_view fmt, uint32_t pid, DDConnectionId umd_connection_id, Args&&... args);

        /// @brief Logs formatted warning message.
        /// @param [in] fmt The format string.
        /// @param [in] pid The process id.
        /// @param [in] umd_connection_id The umd connection id.
        /// @param [in] args The variable arguments to print in formatted message.
        /// @code{.cpp}
        /// logger->LogWarning("Example print: {}", 0, "My message");
        /// @endcode
        template <typename... Args>
        void LogWarning(std::string_view fmt, uint32_t pid, DDConnectionId umd_connection_id, Args&&... args);

        /// @brief Logs formatted info message.
        /// @param [in] fmt The format string.
        /// @param [in] pid The process id.
        /// @param [in] umd_connection_id The umd connection id.
        /// @param [in] args The variable arguments to print in formatted message.
        /// @code{.cpp}
        /// logger->LogInfo("Example print: {}", 0, "My message");
        /// @endcode
        template <typename... Args>
        void LogInfo(std::string_view fmt, uint32_t pid, DDConnectionId umd_connection_id, Args&&... args);

        /// @brief Logs formatted verbose message.
        /// @param [in] fmt The format string.
        /// @param [in] pid The process id.
        /// @param [in] umd_connection_id The umd connection id.
        /// @param [in] args The variable arguments to print in formatted message.
        /// @code{.cpp}
        /// logger->LogVerbose("Example print: {}", 0, "My message");
        /// @endcode
        template <typename... Args>
        void LogVerbose(std::string_view fmt, uint32_t pid, DDConnectionId umd_connection_id, Args&&... args);

    protected:
        /// @brief Append message to log with verbose log level.
        /// @param [in] text The string message.
        /// @param [in] pid The PID that this logging message is associated with.
        /// @param [in] umd_connection_id The UMD connection ID that this logging message is associated with.
        virtual void Verbose(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const = 0;

        /// @brief Append message to log with info log level.
        /// @param [in] text The string message.
        /// @param [in] pid The PID that this logging message is associated with.
        /// @param [in] umd_connection_id The UMD connection ID that this logging message is associated with.
        virtual void Info(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const = 0;

        /// @brief Append message to log with warning log level.
        /// @param [in] text The string message.
        /// @param [in] pid The PID that this logging message is associated with.
        /// @param [in] umd_connection_id The UMD connection ID that this logging message is associated with.
        virtual void Warning(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const = 0;

        /// @brief Append message to log with error log level.
        /// @param [in] text The string message.
        /// @param [in] pid The PID that this logging message is associated with.
        /// @param [in] umd_connection_id The UMD connection ID that this logging message is associated with.
        virtual void Error(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const = 0;
    };

    template <typename... Args>
    void Logger::LogError(const std::string_view fmt, const uint32_t pid, const DDConnectionId umd_connection_id, Args&&... args)
    {
        Error(fmt::vformat(fmt, fmt::make_format_args(args...)), pid, umd_connection_id);
    }

    template <typename... Args>
    void Logger::LogWarning(const std::string_view fmt, const uint32_t pid, const DDConnectionId umd_connection_id, Args&&... args)
    {
        Warning(fmt::vformat(fmt, fmt::make_format_args(args...)), pid, umd_connection_id);
    }

    template <typename... Args>
    void Logger::LogInfo(const std::string_view fmt, const uint32_t pid, const DDConnectionId umd_connection_id, Args&&... args)
    {
        Info(fmt::vformat(fmt, fmt::make_format_args(args...)), pid, umd_connection_id);
    }

    template <typename... Args>
    void Logger::LogVerbose(const std::string_view fmt, const uint32_t pid, const DDConnectionId umd_connection_id, Args&&... args)
    {
        Verbose(fmt::vformat(fmt, fmt::make_format_args(args...)), pid, umd_connection_id);
    }

    /// @brief A logger that does not write any messages to anywhere.
    class NoopLogger : public Logger
    {
    public:
        /// @brief Constructor.
        DIP(NoopLogger()) = default;
        std::shared_ptr<Logger> WithSource(const std::string& source) override;

    protected:
        void Verbose(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;
        void Info(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;
        void Warning(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;
        void Error(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;
    };

    /// @brief Logger that prints to stdout and stderr
    class StdIoLogger : public Logger
    {
    public:
        /// @brief Constructor.
        DIP(StdIoLogger());

        /// @brief Constructor.
        /// @param [in] use_std_err true if standard error should be used for error messages.
        explicit StdIoLogger(bool use_std_err = false);

    private:
        /// @brief Constructor.
        /// @param [in] source The name to prepend to logging messages.
        /// @param [in] use_std_err true if standard error should be used for error messages.
        explicit StdIoLogger(std::string source, bool use_std_err = false);

    public:
        std::shared_ptr<Logger> WithSource(const std::string& source) override;

    protected:
        void Verbose(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;
        void Info(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;
        void Warning(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;
        void Error(const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const override;

    private:
        /// @brief Formats a string to be logged.
        /// @param [in] level_str A string describing the logging level.
        /// @param [in] text The text of the log.
        /// @param [in] pid The associated PID of the message.
        /// @param [in] umd_connection_id The associated UMD connection ID of the message.
        /// @return The formatted string.
        std::string FormatMessage(const std::string& level_str, const std::string& text, uint32_t pid, DDConnectionId umd_connection_id) const;

    public:
        /// @brief Gets the current system time as a string.
        /// @return The current system time as a string.
        inline static std::string GetTimeAsString();

    private:
        std::string source_;               ///< The source name to prepend to logging messages.
        bool        use_std_err_ = false;  ///< true if standard error should be used for error messages.
    };

    std::string StdIoLogger::GetTimeAsString()
    {
        std::chrono::system_clock::time_point time = std::chrono::system_clock::now();
        return fmt::format("{:%Y-%m-%d %H:%M}.{:%S}", time, time.time_since_epoch());
    }

};  // namespace devtrace

#endif
