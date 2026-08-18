// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Recent trace model base class implementation

#include "model/recent_trace_model_base.h"

#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <memory>
#include <utility>

#include "formatting.h"

RecentTraceModelBase::RecentTraceModelBase(QString extension, QObject* parent)
    : QAbstractTableModel(parent)
    , extension_(std::move(extension))
    , file_watcher_(new QFileSystemWatcher)
{
    connect(file_watcher_.get(), &QFileSystemWatcher::directoryChanged, this, &RecentTraceModelBase::OnDirectoryChanged);
    connect(file_watcher_.get(), &QFileSystemWatcher::fileChanged, this, &RecentTraceModelBase::OnFileChanged);
}

void RecentTraceModelBase::SetRootPath(const QString& path)
{
    root_path_ = path;
    Load();
}

QString RecentTraceModelBase::FilePath(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount({}))
    {
        return "";
    }

    return files_.at(index.row()).absoluteFilePath();
}

void RecentTraceModelBase::Load()
{
    // Remove all existing watched paths
    if (const auto paths = file_watcher_->files() + file_watcher_->directories(); !paths.isEmpty())
    {
        file_watcher_->removePaths(paths);
    }

    const QDir root(root_path_);
    if (!root.exists())
    {
        return;
    }

    file_watcher_->addPath(root_path_);

    beginResetModel();

    files_.clear();
    // Using the root path, query all files contained in that path.
    QDirIterator iterator(root);
    while (iterator.hasNext())
    {
        if (const QFileInfo file_info = iterator.nextFileInfo(); file_info.isFile() && file_info.suffix() == extension_)
        {
            files_.push_back(file_info);

            file_watcher_->addPath(file_info.filePath());
        }
    }

    endResetModel();
}

void RecentTraceModelBase::OnFileChanged(const QString& file_path)
{
    if (const QFileInfo info(file_path); !info.exists())
    {
        // Re-load to handle either a re-name or a deletion as both are destructive
        Load();
    }
}

void RecentTraceModelBase::OnDirectoryChanged(const QString& dir_path)
{
    if (const QDir dir(dir_path); dir.count() > files_.count())
    {
        // Something must have been added that is not currently being watched.
        // Do a complete re-load so we grab it
        Load();
    }
}

Qt::ItemFlags RecentTraceModelBase::flags(const QModelIndex& index) const
{
    if (index.column() == kRecentTraceModelNameColumn)
    {
        return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
    }

    return QAbstractTableModel::flags(index);
}

bool RecentTraceModelBase::removeRows(const int row, const int count, const QModelIndex& parent)
{
    const int last = row + (count - 1);
    beginRemoveRows(parent, row, last);
    for (auto i = row; i <= last; i++)
    {
        const auto& info = files_.at(i);

        const QString path = info.absoluteFilePath();
        QFile::remove(path);
    }
    files_.remove(row, count);
    endRemoveRows();

    return true;
}

int RecentTraceModelBase::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return static_cast<int>(files_.size());
}

int RecentTraceModelBase::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return kRecentTraceModelColumnCount;
}

QVariant RecentTraceModelBase::data(const QModelIndex& index, const int role) const
{
    Q_UNUSED(index)
    Q_UNUSED(role)

    const QFileInfo& file_info = files_.at(index.row());
    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
        case kRecentTraceModelNameColumn:
            return file_info.fileName();
        case kRecentTraceModelSizeColumn:
            return Formatting::FormatBytesPow2(file_info.size());
        case kRecentTraceModelDateColumn:
            return file_info.metadataChangeTime();

        default:
            break;
        }
    }
    else if (role == Qt::EditRole)
    {
        switch (index.column())
        {
        case kRecentTraceModelNameColumn:
            return file_info.fileName();

        default:
            break;
        }
    }
    else if (role == Qt::UserRole)
    {
        switch (index.column())
        {
        case kRecentTraceModelNameColumn:
            return file_info.fileName();
        case kRecentTraceModelSizeColumn:
            return file_info.size();
        case kRecentTraceModelDateColumn:
            return file_info.metadataChangeTime();

        default:
            break;
        }
    }

    return {};
}

bool RecentTraceModelBase::setData(const QModelIndex& index, const QVariant& value, const int role)
{
    if (value.toString().isEmpty())
    {
        return false;
    }

    const QFileInfo file_info = files_.at(index.row());
    if (index.column() == kRecentTraceModelNameColumn)
    {
        // Ensure that the proper extension is applied when modifying the filename
        std::filesystem::path root     = root_path_.toStdString();
        std::filesystem::path filename = value.toString().toStdString();
        if (!filename.has_extension() || filename.extension() != extension_.toStdString())
        {
            filename.replace_extension(extension_.toStdString());
        }
        root.append(filename.string());

        const QString path = file_info.absoluteFilePath();

        // Check for name collision
        for (qsizetype idx = 0; idx < files_.size(); ++idx)
        {
            if (index.row() == idx)
                continue;
            if (QString::compare(files_[idx].baseName(), value.toString().split(".").at(0), Qt::CaseInsensitive) == 0)
            {
                emit FileNameValidationError("Name collision. File names must be unique");
                return false;
            }
        }

        // Check for invalid characters.
        // Note: written with escapes rather than a raw string literal. The open source code
        // sanitizer runs unifdef over this file, and unifdef has no notion of raw strings, so the
        // embedded quote reads as an unterminated string literal and truncates the sanitized file.
        QRegularExpression invalidChars("[<>:\"/\\\\|?*]");
        if (QRegularExpressionMatch match = invalidChars.match(value.toString()); match.hasMatch())
        {
            auto error = QString("Forbidden symbol(s) %1 found in file name").arg(match.captured());
            emit FileNameValidationError(error);
            return false;
        }

        QFile      file(path);
        const bool success = file.rename(root);
        if (success)
        {
            files_[index.row()] = QFileInfo(file);
        }

        return success;
    }

    return QAbstractTableModel::setData(index, value, role);
}

QVariant RecentTraceModelBase::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return {};
    }

    switch (section)
    {
    case kRecentTraceModelNameColumn:
        return "Name";
    case kRecentTraceModelSizeColumn:
        return "Size";
    case kRecentTraceModelDateColumn:
        return "Date Modified";

    default:
        break;
    }

    return {};
}
