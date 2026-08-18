// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for the IO objects for trace sources.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_MODEL_QT_TRACE_IO_H_
#define RDP_SOURCE_MODULES_COMMON_INC_MODEL_QT_TRACE_IO_H_

#include <memory>

#include <QDir>
#include <QFile>
#include <QString>

#include <amdrdf.h>

#include <trace_io.h>

/// @brief A writable stream that is backed by a QFile.
class QFileStream : public devtrace::ReadWriteStream
{
public:
    /// @brief Constructor.
    /// @param [in] path The path on disk that the stream should write to.
    explicit QFileStream(const QString& path);

    /// @brief Destructor.
    ~QFileStream() override = default;

    /// @brief Sets the open mode of the stream
    /// @param [in] flags The flags to open the file with.
    void SetOpenMode(QIODevice::OpenModeFlag flags);

    /// @brief Opens the stream.
    /// @return true if the stream was opened, false otherwise.
    bool Open() override;

    // Stream

    /// @brief Closes the stream after writing is complete.
    void Close() override;

    // ReadableStream

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

    // WritableStream

    /// @brief Writes data.
    /// @param [in] count The number of bytes to write.
    /// @param [in] buffer The data that should be written.
    /// @param [out] The number of bytes that were written.
    /// @return true if writing was successful, false otherwise.
    bool Write(const std::int64_t count, const void* buffer, std::int64_t* bytes_written) override;

private:
    QFile file_;               ///< The file that's backing this stream.
    bool  can_write_ = false;  ///< true if the stream can be written to.
    bool  can_read_  = false;  ///< true if the stream can be read from.

    QIODevice::OpenModeFlag open_flags_ = QIODevice::ReadOnly;  ///< The flags to open the file with
};

/// @brief A stream provider that uses the file system to get streams.
class FileSystemStreamProvider : public devtrace::ReadWriteStreamProvider, public devtrace::ReadWriteStreamOpener
{
public:
    /// @brief Destructor.
    ~FileSystemStreamProvider() override = default;

    /// @brief Sets the path that files will be created at.
    /// @param [in] path The new path that files will be created at.
    virtual void SetPath(const QString& path) = 0;

    /// @brief Sets the name of the application that streams are being generated for.
    /// @param [in] app_name The name of the application.
    virtual void SetAppName(const QString& app_name) = 0;
};

/// @brief A writable stream provider that makes streams backed by a QFile.
class QFileStreamProvider : public FileSystemStreamProvider
{
public:
    /// @brief Constructor.
    /// @param [in] extension The file extension to use for filenames.
    QFileStreamProvider(QString extension);

    /// @brief Destructor.
    ~QFileStreamProvider() override = default;

    std::unique_ptr<devtrace::ReadWriteStream> CreateReadWriteStream(std::string& path) override;
    std::unique_ptr<devtrace::ReadWriteStream> OpenReadWriteStream(const std::string& path) override;

    /// @brief Sets the path that files will be created at.
    /// @param [in] path The new path that files will be created at.
    void SetPath(const QString& path) override;

    /// @brief Sets the name of the application that streams are being generated for.
    /// @param [in] app_name The name of the application.
    void SetAppName(const QString& app_name) override;

    const QString extension_;  ///< The file extension to use for filenames.
    QString       app_name_;   ///< The name of the application that streams are being generated for.
    QString       path_;       ///< The current output path for files.
};

#endif
