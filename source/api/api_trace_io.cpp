// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for mock DevTrace IO.

#include "api_trace_io.h"
#include "api_allocator.h"

#include <string.h>

static constexpr size_t kInitialBufferSize = 1024;

std::unique_ptr<devtrace::ReadWriteStream> MemoryReadWriteStreamProvider::CreateReadWriteStream(std::string& path)
{
    return CreateStream(path);
}

std::unique_ptr<devtrace::WritableStream> MemoryReadWriteStreamProvider::CreateWritableStream(std::string& path)
{
    return CreateStream(path);
}

std::unique_ptr<devtrace::ReadWriteStream> MemoryReadWriteStreamProvider::CreateStream(std::string& path)
{
    std::lock_guard lock(state_mutex_);

    path = std::to_string(stream_index_++);

    std::shared_ptr<StreamData> stream_data = std::make_shared<StreamData>();
    stream_data->capacity                   = kInitialBufferSize;
    stream_data->buffer                     = reinterpret_cast<uint8_t*>(ApiAlloc(stream_data->capacity));
    stream_data->size                       = 0;

    data_.insert({path, stream_data});
    return std::make_unique<MemoryReadWriteStream>(stream_data);
}

std::unique_ptr<devtrace::ReadWriteStream> MemoryReadWriteStreamProvider::OpenReadWriteStream(const std::string& path)
{
    std::lock_guard lock(state_mutex_);
    if (data_.count(path) == 0)
    {
        return nullptr;
    }

    return std::make_unique<MemoryReadWriteStream>(data_[path]);
}

bool MemoryReadWriteStreamProvider::ConsumeStream(const std::string& path, uint64_t& size, uint8_t*& data)
{
    std::lock_guard lock(state_mutex_);
    if (data_.count(path) == 0 || data_[path]->open)
    {
        return false;
    }

    auto node = data_.extract(path);
    size      = static_cast<uint64_t>(node.mapped()->size);
    data      = node.mapped()->buffer;

    return true;
}

MemoryReadWriteStream::MemoryReadWriteStream(const std::shared_ptr<StreamData>& data)
    : data_(data)
{
    [[maybe_unused]] const bool was_open = data_->open.exchange(true);
    DEV_TRACE_ASSERT(!was_open);
}

bool MemoryReadWriteStream::Open()
{
    return true;
}

void MemoryReadWriteStream::Close()
{
    data_->open = false;
}

bool MemoryReadWriteStream::Write(const std::int64_t count, const void* buffer, std::int64_t* bytes_written)
{
    const size_t next_position = current_index_ + count;

    bool needs_realloc = false;
    while (next_position >= data_->capacity)
    {
        data_->capacity *= 2;
        needs_realloc = true;
    }

    if (needs_realloc)
    {
        data_->buffer = reinterpret_cast<uint8_t*>(ApiRealloc(data_->buffer, data_->capacity));
    }

    if (next_position > static_cast<size_t>(data_->size))
    {
        data_->size = next_position;
    }

    memcpy(data_->buffer + current_index_, buffer, count);
    current_index_ = next_position;

    if (bytes_written != nullptr)
    {
        *bytes_written = count;
    }

    return true;
}
bool MemoryReadWriteStream::Read(const std::int64_t count, void* buffer, std::int64_t* bytes_read)
{
    const size_t end_position = current_index_ + count;
    if (end_position > static_cast<size_t>(data_->size))
    {
        return false;
    }

    memcpy(buffer, data_->buffer + current_index_, count);
    current_index_ = end_position;

    if (bytes_read != nullptr)
    {
        *bytes_read = count;
    }

    return true;
}
bool MemoryReadWriteStream::Tell(std::int64_t* position)
{
    *position = static_cast<std::int64_t>(current_index_);
    return true;
}
bool MemoryReadWriteStream::Seek(std::int64_t position)
{
    if (position > static_cast<int64_t>(data_->size))
    {
        return false;
    }

    current_index_ = static_cast<size_t>(position);
    return true;
}
bool MemoryReadWriteStream::GetSize(std::int64_t* size)
{
    *size = static_cast<std::int64_t>(data_->size);
    return true;
}
