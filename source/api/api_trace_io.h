// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for API IO.

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <dipper.h>
#include <trace_io.h>

#ifndef SOURCE_API_CAPTURE_TRACE_IO_
#define SOURCE_API_CAPTURE_TRACE_IO_

/// @brief Data for a stream.
struct StreamData
{
    uint8_t* buffer   = nullptr;  ///< The raw buffer.
    size_t   capacity = 0;        ///< The capacity of the buffer.
    int64_t  size     = 0;        ///< The number of bytes that have been written to the buffer.

    std::atomic_bool open = false;  ///< true if the stream is open, false otherwise.
};

/// @brief A writable stream provider that stores stuff in memory.
class MemoryReadWriteStreamProvider : public devtrace::ReadWriteStreamProvider, public devtrace::ReadWriteStreamOpener
{
public:
    DIP(MemoryReadWriteStreamProvider()) = default;

    /// @brief Destructor.
    ~MemoryReadWriteStreamProvider() override = default;

    std::unique_ptr<devtrace::ReadWriteStream> CreateReadWriteStream(std::string& path) override;
    std::unique_ptr<devtrace::WritableStream>  CreateWritableStream(std::string& path) override;
    std::unique_ptr<devtrace::ReadWriteStream> OpenReadWriteStream(const std::string& path) override;

    /// @brief Consumes the stream that has already been created.
    /// @param [in] path The path of the stream to consume.
    /// @param [out] size The size of the buffer.
    /// @param [out] data The data to pointer.
    /// @return true if the stream was found, false otherwise.
    bool ConsumeStream(const std::string& path, uint64_t& size, uint8_t*& data);

private:
    /// @brief Creates a new mock stream.
    /// @param [out] path The path on disk that this stream corresponds to. This should be UTF-8 encoded.
    /// @return A new mock stream.
    std::unique_ptr<devtrace::ReadWriteStream> CreateStream(std::string& path);

    std::mutex                                                   state_mutex_;       ///< Mutex that guards the internal state.
    std::unordered_map<std::string, std::shared_ptr<StreamData>> data_;              ///< Buffers that contain data  for streams.
    uint64_t                                                     stream_index_ = 0;  ///< Index of the next stream.
};

/// @brief A stream that writes to memory.
class MemoryReadWriteStream : public devtrace::ReadWriteStream
{
public:
    /// @brief Creates a new read write stream in memory.
    /// @param [in] data The stream data to write to.
    MemoryReadWriteStream(const std::shared_ptr<StreamData>& data);

    ~MemoryReadWriteStream() override = default;

    bool Open() override;
    void Close() override;

    bool Write(const std::int64_t count, const void* buffer, std::int64_t* bytes_written) override;
    bool Read(const std::int64_t count, void* buffer, std::int64_t* bytes_read) override;
    bool Tell(std::int64_t* position) override;
    bool Seek(std::int64_t position) override;
    bool GetSize(std::int64_t* size) override;

private:
    std::shared_ptr<StreamData> data_;               ///< The stream data to write to.
    size_t                      current_index_ = 0;  ///< The current head index.
};

#endif
