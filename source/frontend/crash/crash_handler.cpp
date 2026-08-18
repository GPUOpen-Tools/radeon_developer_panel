// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP crash handler implementation.

#include <QDateTime>
#include <QStandardPaths>
#include <QTextStream>

#include "../logging/logging_manager.h"
#include "../models/system_info_model.h"
#include "crash_handler.h"

namespace rdp
{
    /// @brief The format of the date in crash folder name.
    static constexpr char const* kCrashFolderDateFormat = "yyyyMMdd-HHmmss";

    /// @brief The name of the folder in the documents folder where crashes are stored
    static constexpr char const* kCrashFolder = "RadeonDeveloperPanel";

    /// @brief The file name for the system info dump.
    static constexpr char const* kSystemInfoFilename = "system_info.json";

    /// @brief The file name for the log file.
    static constexpr char const* kLogFilename = "log.txt";

    CrashHandler::CrashHandler(const std::shared_ptr<SystemInfoModel>& system_info_model)
    {
        connect(system_info_model.get(), &SystemInfoModel::SystemInfoChanged, this, &CrashHandler::SystemInfoChanged);
    }

    void CrashHandler::Handle(const CrashInfo& info)
    {
        ExtendedCrashInfo extended_info(info);

        QString base_folder        = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QDir::separator() + kCrashFolder;
        QString output_folder      = QString("crash-%1").arg(QDateTime::currentDateTime().toString(kCrashFolderDateFormat));
        QString output_folder_path = QDir::toNativeSeparators(base_folder + QDir::separator() + output_folder);

        extended_info.output_directory = QDir(output_folder_path);
        extended_info.output_directory.mkpath(".");

        if (!system_info_json_.isEmpty())
        {
            QString system_info_path = extended_info.output_directory.filePath(kSystemInfoFilename);
            QFile   file(system_info_path);
            if (file.open(QIODevice::ReadWrite))
            {
                QTextStream stream(&file);
                stream << system_info_json_;
                file.close();
            }
        }

        if (LoggingManager::Instance().GetLoggingModel()->Flush())
        {
            QFile::copy(LoggingManager::Instance().GetLogFilePath(), extended_info.output_directory.filePath(kLogFilename));
        }

        Handle(extended_info);
    }

    void CrashHandler::Handle(ExtendedCrashInfo& crash_info)
    {
        Q_UNUSED(crash_info)
        // No-op, should be overridden by an OS specific child class
    }

    void CrashHandler::SystemInfoChanged(const QString& system_info_json)
    {
        system_info_json_ = system_info_json;
    }

}  // namespace rdp
