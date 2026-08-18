// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definitions for debug log model.

#ifndef RDP_SOURCE_FRONTEND_LOGGING_LOGGING_MODEL_H_
#define RDP_SOURCE_FRONTEND_LOGGING_LOGGING_MODEL_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QAbstractItemModel>
#include <QDateTime>
#include <QFile>
#include <QString>
#include <QTextStream>

#include <common/inc/api/dev_tools_logging.h>

#include <dd_common_api.h>
#include "available_logging_filters_model.h"

namespace rdp
{
    /// Enumeration for built-in log sources
    enum class LogSource : uint32_t
    {
        kQt,
        kTool,
        kRouter,
        kPanel,
        kTerminal,
        kModule,
        kCount
    };

    /// @brief The different levels of logging severity.
    enum class LogLevel : uint8_t
    {
        kVerbose = 0,
        kInfo,
        kWarning,
        kError
    };

    /// @brief Converts the log level to a user-readable string.
    /// @param [in] log_level The log level to get the user readable string for.
    /// @return A user readable string describing the log level.
    [[nodiscard]] inline QString LogLevelToString(LogLevel log_level)
    {
        switch (log_level)
        {
        case LogLevel::kVerbose:
            return "VERBOSE";
        case LogLevel::kInfo:
            return "INFO";
        case LogLevel::kWarning:
            return "WARNING";
        case LogLevel::kError:
            return "ERROR";
        default:
            return "UNKNOWN";
        }
    }

    // @brief A log message.
    struct LogMessage
    {
        QString        message;                      ///< The contents of the logging message.
        QString        source;                       ///< The source of the logging message or empty string if it is a generic log message.
        uint32_t       pid    = kLoggingInvalidPid;  ///< The PID that this log message originated from or kLoggingInvalidPid if it did not belong to a process.
        DDConnectionId umd_id = kLoggingInvalidUmdId;  ///< The UMD connection id associated with the log message or kLoggingInvalidUmdId if it is unkown.

        LogLevel  log_level = LogLevel ::kInfo;  ///< The severity of the log message.
        QDateTime timestamp;                     ///< The time that the logging message was created at.
    };

    /// @brief Model that stores all of the logging items.
    class LoggingModel : public QAbstractItemModel
    {
        Q_OBJECT
    public:
        /// @brief Constructor.
        LoggingModel();

        /// @brief Destructor.
        ~LoggingModel() override;

        /// @brief Opens the log file.
        /// @param [in] file_path The path of the log file to open.
        /// @return true if opening the log file was successful, false otherwise.
        bool OpenLogFile(const QString& file_path);

        /// @brief Append message to log with.
        /// @param [in] source The source of this logging message or empty string if it is a generic logging message.
        /// @param [in] text The string message.
        /// @param [in] pid The PID that this logging message is associated with or kLoggingInvalidPid if it is not associated with a process.
        /// @param [in] umd_connection_id The UMD connection id or kLoggingInvalidUmdId if not applicable.
        /// @param [in] level The level the severity level for the log message.
        void AddMessage(const QString& source, const QString& text, uint32_t pid, DDConnectionId umd_connection_id, LogLevel level);

        /// @brief Gets the message at the given row.
        /// @param [in] row The row of the message to get.
        LogMessage GetMessageAtRow(int row);

        /// @brief Writes the current log file to the file but does not close the file.
        /// @return true if flushing to the file was successful, false otherwise.
        bool Flush();

        /// @brief Gets the model that has all of the available PID filters.
        /// @return The model that has all of the available PID filters.
        const std::shared_ptr<AvailableLoggingFiltersPidModel>& GetPidFilterModel() const;

        /// @brief Gets the model that has all of the available source filters.
        /// @return The model that has all of the available source filters.
        const std::shared_ptr<AvailableLoggingFiltersSourceModel>& GetSourceFilterModel() const;

        /// @brief Gets the model that has all of the available UMD connection id filters.
        /// @return The model that has all of the available UMD connection id filters.
        const std::shared_ptr<AvailableLoggingFiltersUmdIdModel>& GetUmdIdFilterModel() const;

        /// @brief Gets the path of the log file or empty string if there is no log file.
        /// @return The path of the log file or empty string if there is no log file.
        QString GetLogFilePath();

    signals:
        /// @brief Emitted when the log file path changes.
        /// @param [in] path The new log file path, or empty if no log file is open.
        void LogFilePathChanged(const QString& path);

    private:
        std::recursive_mutex    messages_mutex_;  ///< The mutex that guards the messages.
        std::vector<LogMessage> message_queue_;   ///< Queue of messages for later insertion.
        std::vector<LogMessage> messages_;        ///< The messages that have been logged.

        QTextStream                  std_out_;          ///< The text stream for stdout.
        std::unique_ptr<QFile>       log_file_;         ///< The file that is logged to.
        std::unique_ptr<QTextStream> log_file_stream_;  ///< The stream used to log to the file.
        QString                      log_file_path_;    ///< The path of the log file or empty string if there is no log file.

        std::shared_ptr<AvailableLoggingFiltersPidModel>    pid_filter_model_;     ///< The model that has all of the available PID filters.
        std::shared_ptr<AvailableLoggingFiltersSourceModel> source_filter_model_;  ///< The model that has all of the available source filters.
        std::shared_ptr<AvailableLoggingFiltersUmdIdModel>  umd_id_filter_model_;  ///< The model that has all of the available UMD connection ID filters.

        std::mutex                                       pid_umd_mutex_;   ///< Guards pid_to_umd_ids_.
        std::unordered_map<uint32_t, std::set<uint16_t>> pid_to_umd_ids_;  ///< Mapping from pid to all observed UMD ids.

    public:
        /// @brief The columns for the model
        enum Columns : uint8_t
        {
            kTimestamp = 0,  ///< The column that has the log timestamp.
            kLevel,          ///< The severity of the log message.
            kSource,         ///< The source of the message.
            kPid,            ///< The PID associated with the log message.
            kUmdId,          ///< The UMD connection Id associated with the log message.
            kMessage,        ///< The actual log message.
            kCount           ///< The total number of columns.
        };

        // QAbstractItemModel

        /// @brief Gets the index at the specified row and column with the given parent.
        /// @param [in] row The row of the index.
        /// @param [in] column The column of the index.
        /// @param [in] parent The parent of the index.
        /// @return The index corresponding to the given row, column and parent.
        QModelIndex index(int row, int column, const QModelIndex& parent) const override;

        /// @brief Gets the parent index for the child.
        /// @param [in] child The child to get the parent index for.
        /// @return The parent index for the child.
        QModelIndex parent(const QModelIndex& child) const override;

        /// @brief Returns the number of rows under the given parent.
        /// @param [in] parent The parent to get the number of children for.
        /// @return The number of children for the parent.
        int rowCount(const QModelIndex& parent) const override;

        /// @brief Returns the number of columns under the given parent.
        /// @param [in] parent The parent to get the number of columns for.
        /// @return The number of columns for the parent.
        int columnCount(const QModelIndex& parent) const override;

        /// @brief Gets the data at the specified index for the given role.
        /// @param [in] index The index to get data for.
        /// @param [in] role The role to look at the data with.
        /// @return The data at the given index for the given role.
        QVariant data(const QModelIndex& index, int role) const override;

        /// @brief Provides the header data for the given section.
        /// @param [in] section The section to get the header data for.
        /// @param [in] orientation The orientation of the header.
        /// @param [in] role The role to look at the data with.
        /// @return The data for the header.
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

        /// @brief Rebuilds UMD filter model from current mapping for a pid (or all when pid invalid) and umd id (or all when umd id invalid).
        /// @param [in] pid The PID to filter model with.
        /// @param [in] umd_id The UMD Id to filter model with.
        void RebuildUmdFilters(uint32_t pid, DDConnectionId umd_id);
    };

}  // namespace rdp

#endif
