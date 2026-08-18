// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the devtrace IO abstraction.

#ifndef RDP_SOURCE_TRACE_INC_TRACE_IO_H_
#define RDP_SOURCE_TRACE_INC_TRACE_IO_H_

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>

#include <amdrdf.h>

namespace devtrace
{
    /// @brief A generic stream.
    class Stream
    {
    public:
        /// @brief Destructor.
        virtual ~Stream() = default;

        /// @brief Opens the stream.
        /// @return true if the stream was opened, false otherwise.
        virtual bool Open() = 0;

        /// @brief Closes the stream.
        virtual void Close() = 0;
    };

    /// @brief A stream that can read data.
    class ReadableStream : public virtual Stream
    {
    public:
        /// @brief Destructor.
        ~ReadableStream() override = default;

        /// @brief Reads data.
        /// @param [in] count The number of bytes to read.
        /// @param [out] buffer The buffer to read the data into.
        /// @param [out] bytes_read The number of bytes that were read.
        /// @return true if reading was successful, false otherwise.
        virtual bool Read(std::int64_t count, void* buffer, std::int64_t* bytes_read) = 0;

        /// @brief Gets the current offset of the stream.
        /// @param [out] position The current offset of the stream.
        /// @return true if getting the offset of the stream was successful, false otherwise.
        virtual bool Tell(std::int64_t* position) = 0;

        /// @brief Seeks the stream.
        /// @param [out] position The position that the stream should be at after seeking.
        /// @return true if seeking the stream was successful, false otherwise.
        virtual bool Seek(std::int64_t position) = 0;

        /// @brief Gets the total size of the stream.
        /// @param [out] size The size in bytes of the stream.
        /// @return true if getting the size of the stream was successful.
        virtual bool GetSize(std::int64_t* size) = 0;
    };

    /// @brief A stream that can write data.
    class WritableStream : public virtual Stream
    {
    public:
        /// @brief Destructor.
        ~WritableStream() override = default;

        /// @brief Writes data.
        /// @param [in] count The number of bytes to write.
        /// @param [in] buffer The data that should be written.
        /// @param [out] The number of bytes that were written.
        /// @return true if writing was successful, false otherwise.
        virtual bool Write(std::int64_t count, const void* buffer, std::int64_t* bytes_written) = 0;
    };

    /// @brief A stream that can be read from and written to.
    class ReadWriteStream : public ReadableStream, public WritableStream
    {
    public:
        /// @brief Gets an rdf userstream for this stream.
        /// @param [out] stream A stream that will read / write to this stream.
        void GetRdfUserStream(rdfUserStream& stream);
    };

    /// @brief Provides writable streams.
    class WritableStreamProvider
    {
    public:
        /// @brief Destructor.
        virtual ~WritableStreamProvider() = default;

        /// @brief Creates a new writable stream.
        /// @param [out] path The path on disk that this stream corresponds to. This should be UTF-8 encoded.
        /// @return A new writable stream.
        virtual std::unique_ptr<WritableStream> CreateWritableStream(std::string& path) = 0;
    };

    /// @brief Provides readable / writable streams.
    class ReadWriteStreamProvider : public WritableStreamProvider
    {
    public:
        /// @brief Destructor.
        ~ReadWriteStreamProvider() override = default;

        /// @brief Creates a new stream.
        /// @param [out] path The path on disk that this stream corresponds to. This should be UTF-8 encoded.
        /// @return A new stream that can be read from and written to.
        virtual std::unique_ptr<ReadWriteStream> CreateReadWriteStream(std::string& path) = 0;

        /// @brief Creates a new writable stream.
        /// @param [out] path The path on disk that this stream corresponds to. This should be UTF-8 encoded.
        /// @return A new writable stream.
        std::unique_ptr<devtrace::WritableStream> CreateWritableStream(std::string& path) override;
    };

    /// @brief Differs from ReadWriteStreamProvider in that this allows specific paths to be opened.
    class ReadWriteStreamOpener
    {
    public:
        /// @brief Destructor.
        virtual ~ReadWriteStreamOpener() = default;

        /// @brief Opens a new stream.
        /// @param [in] path The path on disk that this stream corresponds to.
        /// @return A new stream that can be read from and written to.
        virtual std::unique_ptr<ReadWriteStream> OpenReadWriteStream(const std::string& path) = 0;
    };

};  // namespace devtrace

#endif
