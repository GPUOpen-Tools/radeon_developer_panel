// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for mock DevTrace IO.

#include "mock_trace_io.h"

#include <string.h>

static int stream_index = 0;

std::unique_ptr<devtrace::ReadWriteStream> MockReadWriteStreamProvider::CreateReadWriteStream(std::string& path)
{
    return CreateStream(path);
}

std::unique_ptr<devtrace::WritableStream> MockReadWriteStreamProvider::CreateWritableStream(std::string& path)
{
    return CreateStream(path);
}

bool MockReadWriteStreamProvider::GetIsStreamClosed() const
{
    return closed_;
}

const std::vector<char>& MockReadWriteStreamProvider::GetData() const
{
    return bytes_;
}

const std::string& MockReadWriteStreamProvider::GetPath() const
{
    return current_path_;
}

void MockReadWriteStreamProvider::SetNextStreamsIsBad()
{
    next_stream_is_bad_ = true;
}

void MockReadWriteStreamProvider::SetNextStreamsIsNullptr()
{
    next_stream_is_nullptr_ = true;
}

std::unique_ptr<devtrace::ReadWriteStream> MockReadWriteStreamProvider::CreateStream(std::string& path)
{
    if (!closed_)
    {
        return nullptr;
    }

    current_path_ = "path_" + std::to_string(stream_index++);
    path          = current_path_;

    bytes_.clear();

    if (next_stream_is_nullptr_)
    {
        next_stream_is_nullptr_ = false;
        closed_                 = true;

        return nullptr;
    }

    std::unique_ptr<MockReadWriteStream> stream = std::make_unique<MockReadWriteStream>(bytes_, closed_, next_stream_is_bad_);
    next_stream_is_bad_                         = false;

    return stream;
}

std::unique_ptr<devtrace::ReadWriteStream> MockReadWriteStreamProvider::OpenReadWriteStream(const std::string& path)
{
    if (path != current_path_ || !closed_)
    {
        return nullptr;
    }

    return std::make_unique<MockReadWriteStream>(bytes_, closed_, false);
}

MockReadWriteStream::MockReadWriteStream(std::vector<char>& bytes, bool& closed, bool is_bad)
    : bytes_(bytes)
    , closed_(closed)
    , is_bad_(is_bad)
{
}

bool MockReadWriteStream::Open()
{
    closed_ = false;
    return true;
}

void MockReadWriteStream::Close()
{
    closed_ = true;
}

bool MockReadWriteStream::Write(const std::int64_t count, const void* buffer, std::int64_t* bytes_written)
{
    if (is_bad_)
    {
        return false;
    }

    const size_t next_position = current_index_ + count;
    if (next_position >= bytes_.size())
    {
        bytes_.resize(next_position);
    }

    memcpy(bytes_.data() + current_index_, buffer, count);
    current_index_ = next_position;

    if (bytes_written != nullptr)
    {
        *bytes_written = count;
    }

    return true;
}
bool MockReadWriteStream::Read(const std::int64_t count, void* buffer, std::int64_t* bytes_read)
{
    if (is_bad_)
    {
        return false;
    }

    const size_t end_position = current_index_ + count;
    if (end_position > bytes_.size())
    {
        return false;
    }

    memcpy(buffer, bytes_.data() + current_index_, count);
    current_index_ = end_position;

    if (bytes_read != nullptr)
    {
        *bytes_read = count;
    }

    return true;
}
bool MockReadWriteStream::Tell(std::int64_t* position)
{
    if (is_bad_)
    {
        return false;
    }

    *position = static_cast<std::int64_t>(current_index_);
    return true;
}
bool MockReadWriteStream::Seek(std::int64_t position)
{
    if (position > static_cast<int64_t>(bytes_.size()))
    {
        return false;
    }

    current_index_ = static_cast<size_t>(position);
    return true;
}
bool MockReadWriteStream::GetSize(std::int64_t* size)
{
    *size = static_cast<std::int64_t>(bytes_.size());
    return true;
}
