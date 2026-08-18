// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for debug log model.

#include "logging_model.h"

#include "available_logging_filters_model.h"
#include "utilities.h"

namespace rdp
{
    LoggingModel::LoggingModel()
        : std_out_(stdout)
        , log_file_path_("")
        , pid_filter_model_(new AvailableLoggingFiltersPidModel())
        , source_filter_model_(new AvailableLoggingFiltersSourceModel())
        , umd_id_filter_model_(new AvailableLoggingFiltersUmdIdModel())
    {
    }

    LoggingModel::~LoggingModel()
    {
        log_file_stream_ = nullptr;
        if (log_file_ != nullptr)
        {
            log_file_->close();
            log_file_ = nullptr;
        }

        log_file_path_ = QString("");
    }

    bool LoggingModel::OpenLogFile(const QString& file_path)
    {
        log_file_ = std::make_unique<QFile>(file_path);
        if (log_file_->open(QIODevice::ReadWrite | QIODevice::Truncate))
        {
            log_file_stream_ = std::make_unique<QTextStream>(log_file_.get());
            log_file_path_   = file_path;
            emit LogFilePathChanged(file_path);
            return true;
        }

        log_file_ = nullptr;
        return false;
    }

    void LoggingModel::AddMessage(const QString& source, const QString& text, uint32_t pid, DDConnectionId umd_connection_id, LogLevel level)
    {
        // We don't want to display any new lines in the log widget so we just remove them from the string.
        QString modified_text = text;
        modified_text.replace("\r", " ");
        modified_text.replace("\n", " ");

        const QDateTime message_time   = QDateTime::currentDateTime();
        const QString   message_source = source.isEmpty() ? "RDP" : source;

        {
            const std::lock_guard lock(messages_mutex_);
            message_queue_.push_back({modified_text, message_source, pid, umd_connection_id, level, message_time});
        }

        //The rows need to be inserted after the lock is released to prevent deadlock from trying to acquire a read lock.
        QMetaObject::invokeMethod(this, [&]() {
            const std::lock_guard lock(messages_mutex_);
            if (!message_queue_.empty())
            {
                const int row_insert_index = rowCount({});
                beginInsertRows({}, row_insert_index, row_insert_index + static_cast<int>(message_queue_.size()));
                for (auto& message : message_queue_)
                {
                    messages_.push_back(message);
                }
                endInsertRows();
                message_queue_.clear();
            }
        });

        // Messages should look like this:
        // [16:25:01.532] WARNING (DevDriver): The router unexpectedly lost connection
        const QString formatted_time = message_time.toString("HH:mm:ss.zzz");
        const QString formatted_message =
            pid == kLoggingInvalidPid
                ? QString("(%1) %2 [%3] %4").arg(formatted_time, LogLevelToString(level), message_source, modified_text)
                : QString("(%1) %2 [%3 - PID: %4 - UMD: %5] %6")
                      .arg(formatted_time, LogLevelToString(level), message_source, QString::number(pid), QString::number(umd_connection_id), modified_text);

        {
            const std::lock_guard lock(messages_mutex_);

#ifndef NDEBUG
            std_out_ << formatted_message << Qt::endl;
            std_out_.flush();
#endif

            // Forward message to log file
            if (log_file_stream_ != nullptr)
            {
                *log_file_stream_ << formatted_message << Qt::endl;
            }
        }

        if (pid != kLoggingInvalidPid)
        {
            pid_filter_model_->MessageLogged(pid);
        }
        if (umd_connection_id != kLoggingInvalidUmdId)
        {
            // Maintain pid->umd mapping
            {
                std::lock_guard<std::mutex> map_lock(pid_umd_mutex_);
                pid_to_umd_ids_[pid].insert(umd_connection_id);
            }
            umd_id_filter_model_->MessageLogged(umd_connection_id);
        }

        source_filter_model_->MessageLogged(message_source);
    }

    LogMessage LoggingModel::GetMessageAtRow(int row)
    {
        const std::lock_guard<std::recursive_mutex> lock(messages_mutex_);
        if (row >= static_cast<int>(messages_.size()) || row < 0)
        {
            return {};
        }

        return messages_[row];
    }

    bool LoggingModel::Flush()
    {
        if (log_file_ == nullptr || log_file_stream_ == nullptr)
        {
            return false;
        }

        if (log_file_->isOpen() && log_file_stream_->status() == QTextStream::Status::Ok)
        {
            log_file_stream_->flush();
            return log_file_->flush();
        }

        return false;
    }

    const std::shared_ptr<AvailableLoggingFiltersPidModel>& LoggingModel::GetPidFilterModel() const
    {
        return pid_filter_model_;
    }

    const std::shared_ptr<AvailableLoggingFiltersSourceModel>& LoggingModel::GetSourceFilterModel() const
    {
        return source_filter_model_;
    }

    const std::shared_ptr<AvailableLoggingFiltersUmdIdModel>& LoggingModel::GetUmdIdFilterModel() const
    {
        return umd_id_filter_model_;
    }

    QString LoggingModel::GetLogFilePath()
    {
        return log_file_path_;
    }

    QModelIndex LoggingModel::index(int row, int column, const QModelIndex& parent) const
    {
        const std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(messages_mutex_));
        if (!hasIndex(row, column, parent) || parent.isValid())
        {
            return {};
        }

        return createIndex(row, column);
    }

    QModelIndex LoggingModel::parent(const QModelIndex& child) const
    {
        Q_UNUSED(child);
        return {};
    }

    int LoggingModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid())
        {
            return 0;
        }

        // Qt forces this function to be const, but we need to have a read lock, so we need to do a const cast
        const std::lock_guard lock(const_cast<std::recursive_mutex&>(messages_mutex_));
        return static_cast<int>(messages_.size());
    }

    int LoggingModel::columnCount(const QModelIndex& parent) const
    {
        Q_UNUSED(parent);
        return Columns::kCount;
    }

    QVariant LoggingModel::data(const QModelIndex& index, int role) const
    {
        if (index.parent().isValid())
        {
            return {};
        }

        // Qt forces this function to be const, but we need to have a read lock, so we need to do a const cast
        const std::lock_guard<std::recursive_mutex> lock(const_cast<std::recursive_mutex&>(messages_mutex_));

        Q_ASSERT(index.row() < static_cast<int>(messages_.size()));
        const LogMessage& message = messages_[index.row()];

        if (role == Qt::UserRole)
        {
            return static_cast<int>(message.log_level);
        }
        else if (role != Qt::DisplayRole)
        {
            return {};
        }

        switch (index.column())
        {
        case Columns::kTimestamp:
            return message.timestamp.toString("HH:mm:ss.zzz");
        case Columns::kLevel:
            return LogLevelToString(message.log_level);
        case Columns::kSource:
            return message.source;
        case Columns::kPid:
            return message.pid != kLoggingInvalidPid ? QString::number(message.pid) : "N/A";
        case Columns::kUmdId:
            return message.umd_id != kLoggingInvalidUmdId ? QString::number(message.umd_id) : "N/A";
        case Columns::kMessage:
            return message.message;
        default:
            return {};
        }
    }

    QVariant LoggingModel::headerData(int section, Qt::Orientation orientation, int role) const
    {
        Q_UNUSED(orientation);

        if (role != Qt::DisplayRole)
        {
            return {};
        }

        switch (section)
        {
        case Columns::kTimestamp:
            return "Timestamp";
        case Columns::kLevel:
            return "Level";
        case Columns::kSource:
            return "Source";
        case Columns::kPid:
            return "PID";
        case Columns::kUmdId:
            return "UMD";
        case Columns::kMessage:
            return "Message";
        default:
            return {};
        }
    }

    // New helper to rebuild UMD filter model for a specific pid (or all if invalid)
    void LoggingModel::RebuildUmdFilters(uint32_t pid, DDConnectionId umd_id)
    {
        std::set<DDConnectionId> ids;
        {
            std::lock_guard<std::mutex> map_lock(pid_umd_mutex_);
            if (pid == kLoggingInvalidPid)
            {
                for (auto& kv : pid_to_umd_ids_)
                {
                    ids.insert(kv.second.begin(), kv.second.end());
                }
            }
            else
            {
                if (umd_id == kLoggingInvalidUmdId)
                {
                    auto it = pid_to_umd_ids_.find(pid);
                    if (it != pid_to_umd_ids_.end())
                    {
                        ids = it->second;
                    }
                }
                else
                {
                    ids.insert(umd_id);
                }
            }
        }
        umd_id_filter_model_->ReplaceValues(ids);
    }
}  // namespace rdp
