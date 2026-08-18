// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for global logging utilities.

#include "logging_manager.h"

#include <algorithm>
#include <array>

#include <QDir>
#include <QStandardPaths>

#include <dd_api_registry_api.h>

#include "utilities.h"
#include "version.h"

namespace
{
#ifndef NDEBUG
    void QtDebugLoggingCallback(QtMsgType type, const QMessageLogContext& context, const QString& msg)
    {
        // TODO: Use the context if applicable
        Q_UNUSED(context)

        rdp::LogLevel rdp_level;
        switch (type)
        {
        case QtInfoMsg:
            rdp_level = rdp::LogLevel::kInfo;
            break;
        case QtWarningMsg:
            rdp_level = rdp::LogLevel::kWarning;
            break;
        case QtCriticalMsg:
        case QtFatalMsg:
            rdp_level = rdp::LogLevel::kError;
            break;
        case QtDebugMsg:
        default:
            rdp_level = rdp::LogLevel::kVerbose;
            break;
        }

        std::string message_str = msg.toStdString();
        std::string source      = "QtDebug";

        rdp::LoggingManager::Instance().GetLoggingModel()->AddMessage(source.c_str(), message_str.c_str(), kLoggingInvalidPid, kLoggingInvalidUmdId, rdp_level);
    }
#endif
}  // namespace

namespace rdp
{
    static constexpr uint32_t kMaxFormattedMessageLength = 4096;

    LoggingManager::LoggingManager()
        : logging_model_(new LoggingModel())
    {
#ifndef NDEBUG
        qInstallMessageHandler(QtDebugLoggingCallback);
#endif
    }

    void LoggingManager::Initialize()
    {
        // Attempt to open log file
        const QString log_file_path = util::GetApplicationDataPath().absoluteFilePath(kLogFileName);
        if (logging_model_->OpenLogFile(log_file_path))
        {
            log_file_path_       = log_file_path;
            const QString result = QString("Log file created successfully: [%1]").arg(log_file_path_);
            RDP_LOG_INFO(qUtf8Printable(result));
        }
        else
        {
            const QString result = QString("Failed to create log file: [%1]").arg(log_file_path_);
            RDP_LOG_ERROR(qUtf8Printable(result));
        }

        // Verbose logging information for debugging log dumps
        RDP_LOG_INFO("Initializing RDP {}", RDP_VERSION);

        // System info (OS, hardware, kernel, CPU arch, etc)
        RDP_LOG_INFO("System Info");

        RDP_LOG_INFO("{:<20}: {:>40}", "CPU Architecture", qUtf8Printable(QSysInfo::currentCpuArchitecture()));
        RDP_LOG_INFO("{:<20}: {:>40}", "Kernel Type", qUtf8Printable(QSysInfo::kernelType()));
        RDP_LOG_INFO("{:<20}: {:>40}", "Kernel Version", qUtf8Printable(QSysInfo::kernelVersion()));
        RDP_LOG_INFO("{:<20}: {:>40}", "OS Name", qUtf8Printable(util::PrettyOsName()));
        RDP_LOG_INFO("{:<20}: {:>40}", "Host Name", qUtf8Printable(QSysInfo::machineHostName()));

        // Log compile- and run-time library versions of dependencies, so we can identify mismatches
        RDP_LOG_INFO("Qt Library Versions");

        RDP_LOG_INFO("{:<20}: {:>40}", "Compile-Time", QT_VERSION_STR);
        RDP_LOG_INFO("{:<20}: {:>40}", "Run-Time", qVersion());

        RDP_LOG_INFO("DDRouter Library Versions");

        RDP_LOG_INFO("{:<20}: {:>40}", "Compile-Time", DD_ROUTER_API_VERSION_STRING);
        RDP_LOG_INFO("{:<20}: {:>40}", "Link-Time", ddRouterQueryVersionString());
    }

    DDLoggerApi LoggingManager::GetLoggerCallback()
    {
        return {nullptr, nullptr, nullptr, LogDevDriver};
    }

    void LoggingManager::Register(DDApiRegistry* api_registry)
    {
        DevToolsLoggingApi api{};
        api.info    = &LoggingManager::LogInfo;
        api.warning = &LoggingManager::LogWarning;
        api.error   = &LoggingManager::LogError;
        api.verbose = &LoggingManager::LogVerbose;

        const DD_RESULT result = api_registry->Add(api_registry->pInstance,
                                                   kDevToolsLoggingApiName,
                                                   DDVersion{kDevToolsLoggingApiVersionMajor, kDevToolsLoggingApiVersionMinor, kDevToolsLoggingApiVersionPatch},
                                                   &api,
                                                   sizeof(DevToolsLoggingApi));

        Q_ASSERT(result == DD_RESULT_SUCCESS);
    }

    LoggingManager& LoggingManager::Instance()
    {
        static LoggingManager manager;
        return manager;
    }

    std::shared_ptr<LoggingModel> LoggingManager::GetLoggingModel()
    {
        return logging_model_;
    }

    void LoggingManager::LogInfo(const std::string& source, const std::string& text, uint32_t pid, DDConnectionId umd_connection_id)
    {
        Instance().logging_model_->AddMessage(source.c_str(), text.c_str(), pid, umd_connection_id, LogLevel::kInfo);
    }

    void LoggingManager::LogWarning(const std::string& source, const std::string& text, uint32_t pid, DDConnectionId umd_connection_id)
    {
        Instance().logging_model_->AddMessage(source.c_str(), text.c_str(), pid, umd_connection_id, LogLevel::kWarning);
    }

    void LoggingManager::LogError(const std::string& source, const std::string& text, uint32_t pid, DDConnectionId umd_connection_id)
    {
        Instance().logging_model_->AddMessage(source.c_str(), text.c_str(), pid, umd_connection_id, LogLevel::kError);
    }

    void LoggingManager::LogVerbose(const std::string& source, const std::string& text, uint32_t pid, DDConnectionId umd_connection_id)
    {
        Instance().logging_model_->AddMessage(source.c_str(), text.c_str(), pid, umd_connection_id, LogLevel::kVerbose);
    }

    void LoggingManager::LogDevDriver(DDLoggerInstance* userdata, DD_LOG_LVL level, const char* format, ...)
    {
        Q_UNUSED(userdata);

        std::array<char, kMaxFormattedMessageLength> buffer{};
        va_list                                      arg_ptr;
        va_start(arg_ptr, format);
        vsnprintf(buffer.data(), kMaxFormattedMessageLength, static_cast<const char*>(format), arg_ptr);
        va_end(arg_ptr);

        LogLevel rdp_level;
        switch (level)
        {
        case DD_LOG_LVL_INFO:
            rdp_level = LogLevel::kInfo;
            break;
        case DD_LOG_LVL_WARN:
            rdp_level = LogLevel::kWarning;
            break;
        case DD_LOG_LVL_ERROR:
            rdp_level = LogLevel::kError;
            break;
        case DD_LOG_LVL_VERBOSE:
        default:
            rdp_level = LogLevel::kVerbose;
            break;
        }

        std::string message_str = buffer.data();
        std::string source      = "DevDriver";

        // Since the DevDriver API doesn't include the source, we infer it.
        // DevDriver likes to format messages with [SOURCE] MESSAGE, so if the message starts with a open and close square bracket, we interpret that as
        // the source of the log message.
        if (!message_str.empty())
        {
            const auto open_bracket = std::ranges::find(message_str, '[');
            const auto end_bracket  = std::ranges::find(message_str, ']');

            if (open_bracket == message_str.begin() && end_bracket != message_str.end() && end_bracket + 1 != message_str.end())
            {
                const size_t source_length = std::distance(open_bracket, end_bracket);
                source                     = message_str.substr(1, source_length - 1);

                // Trim leading whitespace
                const auto whitespace_end =
                    std::find_if(end_bracket + 1, message_str.end(), [](unsigned char character) { return std::isspace(character) == 0; });

                const size_t message_start = source_length + std::distance(end_bracket, whitespace_end);
                message_str                = message_str.substr(message_start, message_str.size() - message_start);
            }
        }

        Instance().logging_model_->AddMessage(source.c_str(), message_str.c_str(), kLoggingInvalidPid, kLoggingInvalidUmdId, rdp_level);
    }

}  // namespace rdp
