// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for the IO objects for trace sources.

#include "model/qt_trace_io.h"

#include <QDateTime>
#include <utility>

QFileStream::QFileStream(const QString& path)
    : file_(path)
{
}

void QFileStream::SetOpenMode(const QIODevice::OpenModeFlag flags)
{
    open_flags_ = flags;
}

bool QFileStream::Open()
{
    const bool opened = file_.open(open_flags_);
    if (opened)
    {
        can_read_  = static_cast<bool>(open_flags_ & QIODevice::ReadOnly);
        can_write_ = static_cast<bool>(open_flags_ & QIODevice::WriteOnly);
    }

    return opened;
}

void QFileStream::Close()
{
    file_.close();
    can_read_  = false;
    can_write_ = false;
}

bool QFileStream::Read(const std::int64_t count, void* buffer, std::int64_t* bytes_read)
{
    if (count == 0 || buffer == nullptr || !can_read_ || !file_.isOpen())
    {
        return false;
    }

    const qint64 count_u = count;
    const qint64 read    = file_.read(static_cast<char*>(buffer), count_u);
    if (bytes_read != nullptr)
    {
        *bytes_read = static_cast<std::int64_t>(read);
    }

    return read == count_u;
}

bool QFileStream::Tell(std::int64_t* position)
{
    if (!file_.isOpen())
    {
        return false;
    }

    if (position != nullptr)
    {
        *position = file_.pos();
    }

    return true;
}

bool QFileStream::Seek(const std::int64_t position)
{
    if (!file_.isOpen())
    {
        return false;
    }

    return file_.seek(position);
}

bool QFileStream::GetSize(std::int64_t* size)
{
    if (!file_.isOpen())
    {
        return false;
    }

    if (size != nullptr)
    {
        *size = file_.size();
    }

    return true;
}

bool QFileStream::Write(const std::int64_t count, const void* buffer, std::int64_t* bytes_written)
{
    if (count == 0 || buffer == nullptr || !can_write_ || !file_.isOpen())
    {
        return false;
    }

    const qint64 count_q = count;
    const qint64 written = file_.write(static_cast<const char*>(buffer), count_q);
    if (bytes_written != nullptr)
    {
        *bytes_written = static_cast<std::int64_t>(written);
    }

    return written == count_q;
}

// File stream provider
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QFileStreamProvider::QFileStreamProvider(QString extension)
    : extension_(std::move(extension))
    , app_name_("Unknown")
{
}

std::unique_ptr<devtrace::ReadWriteStream> QFileStreamProvider::CreateReadWriteStream(std::string& path)
{
    if (path_.isEmpty())
    {
        return nullptr;
    }

    const QDateTime now        = QDateTime::currentDateTime();
    const QString   local_time = now.toString("yyyyMMdd-HHmmsszzz");

    const QString filename = QString("%1-%2.%3").arg(app_name_, local_time, extension_);
    const QDir    dir(path_);

    const QString full_path = dir.filePath(filename);
    path                    = full_path.toStdString();

    std::unique_ptr<QFileStream> stream(new QFileStream(full_path));
    stream->SetOpenMode(QIODevice::ReadWrite);

    return stream;
}

void QFileStreamProvider::SetPath(const QString& path)
{
    path_ = path;
}

void QFileStreamProvider::SetAppName(const QString& app_name)
{
    if (app_name.isEmpty())
    {
        app_name_ = "Unknown";
        return;
    }

    const QFileInfo info(app_name);
    app_name_ = info.suffix() == "exe" ? info.completeBaseName() : info.fileName();
}

std::unique_ptr<devtrace::ReadWriteStream> QFileStreamProvider::OpenReadWriteStream(const std::string& path)
{
    std::unique_ptr<QFileStream> stream(new QFileStream(path.c_str()));
    stream->SetOpenMode(QIODevice::ReadWrite);

    return stream;
}
