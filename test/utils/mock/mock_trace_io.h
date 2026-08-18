// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for mock DevTrace IO.

#include <memory>
#include <vector>

#include <trace_io.h>

#ifndef RDP_TEST_UTILS_MOCK_MOCK_TRACE_IO
#define RDP_TEST_UTILS_MOCK_MOCK_TRACE_IO

/// @brief A mock writable stream provider.
///
/// This class is NOT thread safe.
class MockReadWriteStreamProvider : public devtrace::ReadWriteStreamProvider, public devtrace::ReadWriteStreamOpener
{
public:
    /// @brief Destructor.
    ~MockReadWriteStreamProvider() override = default;

    std::unique_ptr<devtrace::ReadWriteStream> CreateReadWriteStream(std::string& path) override;
    std::unique_ptr<devtrace::WritableStream>  CreateWritableStream(std::string& path) override;
    std::unique_ptr<devtrace::ReadWriteStream> OpenReadWriteStream(const std::string& path) override;

public:
    /// @brief Provides whether or not the created stream has been closed.
    /// @return true if the stream has been closed, false otherwise.
    bool GetIsStreamClosed() const;

    /// @brief Gets the data for the created stream.
    /// @return The data for the created stream.
    const std::vector<char>& GetData() const;

    /// @brief Gets the current path for the stream that has been created.
    /// @return The path of the stream.
    const std::string& GetPath() const;

    /// @brief Makes it so that the next created stream will return false from Write() without writing any data.
    void SetNextStreamsIsBad();

    /// @brief Makes it so that the next created stream will be nullptr.
    void SetNextStreamsIsNullptr();

private:
    /// @brief Creates a new mock stream.
    /// @param [out] path The path on disk that this stream corresponds to. This should be UTF-8 encoded.
    /// @return A new mock stream.
    std::unique_ptr<devtrace::ReadWriteStream> CreateStream(std::string& path);

    std::vector<char> bytes_;                           ///< The data for  that has been written to the stream.
    bool              closed_                 = true;   ///< true if the stream has been closed, false otherwise.
    bool              next_stream_is_bad_     = false;  ///< true if the next created stream will return false from Write() without writing any data.
    bool              next_stream_is_nullptr_ = false;  ///< true if the next created stream will be nullptr.
    std::string       current_path_           = "";     ///< The current stream path.
};

/// @brief A mock writable stream.
class MockReadWriteStream : public devtrace::ReadWriteStream
{
public:
    /// @brief Creates a new mock writable stream that will write to the array.
    /// @param [out] bytes The place where the stream will write to.
    /// @param [out] closed A boolean to set to false when the stream is closed.
    /// @param [in] is_bad true if this should return false from Write() without writing any data.
    MockReadWriteStream(std::vector<char>& bytes, bool& closed, bool is_bad);

    /// @brief Destructor.
    ~MockReadWriteStream() override = default;

    bool Open() override;

    /// @brief Marks that the stream has been closed.
    void Close() override;

    /// @brief Writes data.
    /// @param [in] count The number of bytes to write.
    /// @param [in] buffer The data that should be written.
    /// @param [out] The number of bytes that were written.
    /// @return true if writing was successful, false otherwise.
    bool Write(const std::int64_t count, const void* buffer, std::int64_t* bytes_written) override;

    /// @brief Reads data.
    /// @param [in] count The number of bytes to read.
    /// @param [out] buffer The buffer to read the data into.
    /// @param [out] bytes_read The number of bytes that were read.
    /// @return true if reading was successful, false otherwise.
    bool Read(const std::int64_t count, void* buffer, std::int64_t* bytes_read) override;

    /// @brief Gets the current offset of the stream.
    /// @param [out] position The current offset of the stream.
    /// @return true if getting the offset of the stream was successful, false otherwise.
    bool Tell(std::int64_t* position) override;

    /// @brief Seeks the stream.
    /// @param [out] position The position that the stream should be at after seeking.
    /// @return true if seeking the stream was successful, false otherwise.
    bool Seek(std::int64_t position) override;

    /// @brief Gets the total size of the stream.
    /// @param [out] size The size in bytes of the stream.
    /// @return true if getting the size of the stream was successful.
    bool GetSize(std::int64_t* size) override;

private:
    std::vector<char>& bytes_;              ///< The data that has been written to the stream.
    bool&              closed_;             ///< true if the stream has been closed, false otherwise.
    bool               is_bad_;             ///< true if this should return false from Write() without writing any data.
    size_t             current_index_ = 0;  ///< The current head index.
};

#endif
