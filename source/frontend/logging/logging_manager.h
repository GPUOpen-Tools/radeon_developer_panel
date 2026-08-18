// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definitions for global logging utilities.

#ifndef RDP_SOURCE_FRONTEND_LOGGING_MANAGER_H_
#define RDP_SOURCE_FRONTEND_LOGGING_MANAGER_H_

#include <memory>

#include <QTextStream>

#include <ddApi.h>
#include <ddRouter.h>
#include <dd_logger_api.h>
#include <dd_tool_api.h>

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <common/inc/api/dev_tools_logging.h>

#include "logging/logging_model.h"

#ifndef RDP_LOG_VERBOSE
#define RDP_LOG_VERBOSE(message, ...) rdp::LoggingManager::Instance().LogAppend(rdp::LogLevel::kVerbose, message, ##__VA_ARGS__)
#endif

#ifndef RDP_LOG_INFO
#define RDP_LOG_INFO(message, ...) rdp::LoggingManager::Instance().LogAppend(rdp::LogLevel::kInfo, message, ##__VA_ARGS__)
#endif

#ifndef RDP_LOG_WARN
#define RDP_LOG_WARN(message, ...) rdp::LoggingManager::Instance().LogAppend(rdp::LogLevel::kWarning, message, ##__VA_ARGS__)
#endif

#ifndef RDP_LOG_ERROR
#define RDP_LOG_ERROR(message, ...) rdp::LoggingManager::Instance().LogAppend(rdp::LogLevel::kError, message, ##__VA_ARGS__)
#endif

struct DDApiRegistry;

namespace rdp
{
    class LoggingManager
    {
    public:
        /// @brief Initializes the logging manager.
        void Initialize();

        /// @brief Gets a DevDriver logger callback.
        /// @return A DevDriver logger callback.
        static DDLoggerApi GetLoggerCallback();

        /// @brief Registers the logging API in the API registry.
        /// @param [in] api_registry The API to register the logging API in.

        static void Register(DDApiRegistry* api_registry);
        /// @brief Gets global instance to logging manager.
        /// @return logging manager instance.
        static LoggingManager& Instance();

        /// @brief Gets the logging model.
        /// @return The logging model.
        std::shared_ptr<LoggingModel> GetLoggingModel();

        /// @brief Provides the path that the log file is at.
        /// @return The path of the log file on disk.
        const QString& GetLogFilePath()
        {
            return log_file_path_;
        }

        /// @brief Appends formatted log message using variadic parameters
        /// @param [in] level The log level.
        /// @param [in] fmt The formatted message string.
        /// @param [in] args The arguments to append to formatted message.
        template <typename... Args>
        void LogAppend(LogLevel level, const std::string& fmt, Args... args);

    private:
        /// @brief Append message to log with info log level.
        /// @param [in] source The source of this logging message or empty string if it is a generic logging message.
        /// @param [in] text The string message.
        /// @param [in] pid The PID that this logging message is associated with or kLoggingInvalidPid if it is not associated with a process.
        /// @param [in] umd_connection_id The UMD connection id or kLoggingInvalidUmdId if not applicable.
        static void LogInfo(const std::string& source,
                            const std::string& text,
                            uint32_t           pid               = kLoggingInvalidPid,
                            DDConnectionId     umd_connection_id = kLoggingInvalidUmdId);

        /// @brief Append message to log with warning log level.
        /// @param [in] source The source of this logging message or empty string if it is a generic logging message.
        /// @param [in] text The string message.
        /// @param [in] pid The PID that this logging message is associated with or kLoggingInvalidPid if it is not associated with a process.
        /// @param [in] umd_connection_id The UMD connection id or kLoggingInvalidUmdId if not applicable.
        static void LogWarning(const std::string& source,
                               const std::string& text,
                               uint32_t           pid               = kLoggingInvalidPid,
                               DDConnectionId     umd_connection_id = kLoggingInvalidUmdId);

        /// @brief Append message to log with error log level.
        /// @param [in] source The source of this logging message or empty string if it is a generic logging message.
        /// @param [in] text The string message.
        /// @param [in] pid The PID that this logging message is associated with or kLoggingInvalidPid if it is not associated with a process.
        /// @param [in] umd_connection_id The UMD connection id or kLoggingInvalidUmdId if not applicable.
        static void LogError(const std::string& source,
                             const std::string& text,
                             uint32_t           pid               = kLoggingInvalidPid,
                             DDConnectionId     umd_connection_id = kLoggingInvalidUmdId);

        /// @brief Append message to log with verbose log level.
        /// @param [in] source The source of this logging message or empty string if it is a generic logging message.
        /// @param [in] text The string message.
        /// @param [in] pid The PID that this logging message is associated with or kLoggingInvalidPid if it is not associated with a process.
        /// @param [in] umd_connection_id The UMD connection id or kLoggingInvalidUmdId if not applicable.
        static void LogVerbose(const std::string& source,
                               const std::string& text,
                               uint32_t           pid               = kLoggingInvalidPid,
                               DDConnectionId     umd_connection_id = kLoggingInvalidUmdId);

        /// @brief Called when DevDriver logs a message.
        /// @param [in] userdata nullptr.
        /// @param [in] level The level of the message.
        /// @param [in] format The format of the message.
        static void LogDevDriver(DDLoggerInstance* userdata, DD_LOG_LVL level, const char* format, ...);

    private:
        /// @brief Constructor.
        LoggingManager();

        std::shared_ptr<LoggingModel> logging_model_;  ///< The logging model.

        constexpr static const char* kLogFileName = "log.txt";       ///< log filename
        QString                      log_file_path_;                 ///< The path of the logfile.
        QtMessageHandler             qt_message_handler_ = nullptr;  ///< Qt message handler.
    };

    template <typename... Args>
    void LoggingManager::LogAppend(const LogLevel level, const std::string& fmt, Args... args)
    {
        const std::string message = fmt::vformat(fmt, fmt::make_format_args(args...));
        switch (level)
        {
        case LogLevel::kError:
            LogError("", message);
            break;
        case LogLevel::kInfo:
            LogInfo("", message);
            break;
        case LogLevel::kVerbose:
            LogVerbose("", message);
            break;
        case LogLevel::kWarning:
            LogWarning("", message);
            break;

        default:
            break;
        }
    }

}  // namespace rdp

#endif
