// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration file writing utilities.

#ifndef RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_FILE_WRITING_H_
#define RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_FILE_WRITING_H_

#include <memory>
#include <mutex>

#include <ddApi.h>
#include <ddRdf.h>

#include "trace_io.h"

namespace devtrace
{

    /// @brief The progress of writing a file.
    struct WritingProgress
    {
        /// @brief The number of bytes that will be dumped.
        uint64_t total_bytes_to_dump = 0;

        /// @brief The number of bytes that have been dumped.
        uint64_t num_bytes_dumped = 0;

        /// @brief A number [0.0, 1.0] that represents the current progress.
        float progress = 0.0;
    };

    struct WritingStatusEventArgs
    {
        bool            is_writing = false;  ///< true if actively writing bytes.
        WritingProgress progress;            ///< Progress of byte writing operations.
    };

    struct WritingStatusEvent
    {
        void*                                                     listener;  ///< event listener object.
        std::function<void(void*, const WritingStatusEventArgs&)> callback;  ///< event callback.
    };

    /// @brief The base interface for a file writer.
    class WriterBase
    {
    public:
        WriterBase();

        /// @brief Destructor.
        virtual ~WriterBase();

        /// @brief Sets the post process operation to run during the end callback if writing was a success.
        /// @param [in] operation The postprocess operation.
        virtual void SetPostProcessOperation(const std::function<void(const std::unique_ptr<ReadWriteStream>& stream)>& operation) = 0;

        /// @brief Registers a writing status event listener with this writer.
        /// @param [in] event The event to register
        virtual void RegisterWritingStatusEvent(const WritingStatusEvent& event);

        /// @brief Emits a writing status event to any registered event listeners.
        /// @param [in] args The event args.
        virtual void EmitWritingStatusEvent(const WritingStatusEventArgs& args);

    protected:
        std::mutex                      status_events_mutex_;  ///< Mutex protecting status event list.
        std::vector<WritingStatusEvent> status_events_;        ///< List of registered status event listeners.
    };

    /// @brief Implements the callbacks for DDByteWriter.
    class ByteWriter : public WriterBase
    {
    public:
        /// @brief Destructor.
        ~ByteWriter() override = default;

        /// @brief Gets the DDByteWriter for this writer.
        /// @return The DDByteWriter for this writer.
        virtual const DDByteWriter& Writer() = 0;
    };

    /// @brief Implements the callbacks for DDRdfFileWriter and DDIOHeartbeat.
    class RdfWriter : public WriterBase
    {
    public:
        /// @brief Destructor.
        ~RdfWriter() override = default;

        /// @brief Gets the DDRdfFileWriter.
        /// @return The DDRdfFileWriter.
        virtual const DDRdfFileWriter& Writer() = 0;

        /// @brief Gets the DDIOHeartbeat.
        /// @return The DDIOHeartbeat.
        virtual const DDIOHeartbeat& Heartbeat() = 0;
    };

    /// @brief The base class for a file writer that uses a trace source.
    class SourceWriterBase final : public WriterBase
    {
    public:
        /// @brief Constructor.
        /// @param [in] writing_mutex The mutex to lock when writing begins.
        /// @param [in] report_progress A function to report progress with.
        SourceWriterBase(std::mutex& writing_mutex, const std::function<void(const WritingProgress&)>& report_progress);

        void SetPostProcessOperation(const std::function<void(const std::unique_ptr<ReadWriteStream>& stream)>& operation) override;

        /// @brief Returns the current progress.
        /// @return The current progress.
        WritingProgress& GetCurrentProgress();

        /// @brief Reports the current progress.
        void ReportCurrentProgress() const;

        /// @brief Begins writing by locking, resetting the progress and reporting it.
        void BeginWriting();

        /// @brief Performs the post-processing operation.
        /// @param [in] stream The stream to perform the operation on.
        void PostProcess(const std::unique_ptr<ReadWriteStream>& stream) const;

    private:
        std::mutex&                                 writing_mutex_;       ///< Mutex to lock when writing begins.
        std::function<void(const WritingProgress&)> report_progress_;     ///< A function to report progress with.
        std::unique_lock<std::mutex>                writing_lock_;        ///< The lock on writing_mutex_.
        WritingProgress                             current_progress_{};  ///< The current progress.

        std::function<void(const std::unique_ptr<ReadWriteStream>& stream)> postprocess_;  ///< The postprocess operation to run on the stream.
    };

    /// @brief Implements the callbacks for DDByteWriter.
    class SourceByteWriter final : public ByteWriter
    {
    public:
        /// @brief Constructor.
        /// @param [in] stream The stream to write to.
        /// @param [in] writing_mutex The mutex to lock when writing begins.
        /// @param [in] report_progress A function to report progress with.
        SourceByteWriter(std::unique_ptr<ReadWriteStream>&                  stream,
                         std::mutex&                                        writing_mutex,
                         const std::function<void(const WritingProgress&)>& report_progress);

        const DDByteWriter& Writer() override;

        void SetPostProcessOperation(const std::function<void(const std::unique_ptr<ReadWriteStream>& stream)>& operation) override;

        void RegisterWritingStatusEvent(const WritingStatusEvent& event) override;

    private:
        /// @brief Callback for the beginning call for a DDByteWriter.
        /// @param [in] userdata The instance of BaseTraceSource the callback is for.
        /// @param [in] total_data_size The total size of the data about to be dumped.
        /// @return DD_RESULT_SUCCESS
        static DD_RESULT ByteWriterBegin(void* userdata, const size_t* total_data_size);

        /// @brief Writes some data to the currently dumping trace file.
        /// @param [in] userdata The instance of BaseTraceSource the callback is for.
        /// @param [in] data The data to write.
        /// @param [in] data_size The number of bytes of data to write.
        /// @return DD_RESULT_SUCCESS if writing is successful, otherwise DD_RESULT_UNKNOWN.
        static DD_RESULT ByteWriterWriteBytes(void* userdata, const void* data, size_t data_size);

        /// @brief Ends writing a trace file.
        /// @param [in] userdata The instance of BaseTraceSource the callback is for.
        /// @param [in] result The final result of the transfer.
        static void ByteWriterEnd(void* userdata, DD_RESULT result);

        SourceWriterBase                 base_;      ///< Base file writer.
        std::unique_ptr<ReadWriteStream> stream_;    ///< The stream to write to.
        DDByteWriter                     writer_{};  ///< The DDByteWriter.
    };

    /// @brief Implements the callbacks for DDRdfFileWriter and DDIOHeartbeat.
    class SourceRdfWriter final : public RdfWriter
    {
    public:
        /// @brief Constructor.
        /// @param [in] stream The stream to write to.
        /// @param [in] writing_mutex The mutex to lock when writing begins.
        /// @param [in] report_progress A function to report progress with.
        SourceRdfWriter(std::unique_ptr<ReadWriteStream>&                  stream,
                        std::mutex&                                        writing_mutex,
                        const std::function<void(const WritingProgress&)>& report_progress);

        const DDRdfFileWriter& Writer() override;
        const DDIOHeartbeat&   Heartbeat() override;

        void SetPostProcessOperation(const std::function<void(const std::unique_ptr<ReadWriteStream>& stream)>& operation) override;

    private:
        /// @brief Handles the beginning of an IO operation.
        /// @param [in] result The result of beginning the operation.
        /// @param [in] total_bytes The estimated total number of bytes.
        /// @return DD_RESULT_SUCCESS if the beginning operation was successful.
        DD_RESULT HandleIoBegin(DD_RESULT result, size_t total_bytes);

        /// @brief Handles when bytes are written during an IO operation.
        /// @param [in] result The result of the write operation.
        /// @param [in] bytes The number of bytes that were written.
        /// @return DD_RESULT_SUCCESS if write was handled successfully.
        DD_RESULT HandleIoWrite(DD_RESULT result, size_t bytes);

        /// @brief Handles the end of an IO operation.
        /// @param [in] result The result of the IO operation.
        /// @return DD_RESULT_SUCCESS.
        DD_RESULT HandleIoEnd(DD_RESULT result) const;

        /// @brief Serves as the heartbeat callback for a DDIOHeartbeat.
        /// @param [in] userdata The pointer to the trace source.
        /// @param [in] result The result of the IO operation thus far.
        /// @param [in] status The current status of something writing.
        /// @param [in] bytes Estimation of the bytes for the current status step. See definition of PFN_ddIOWriteHeartbeat.
        /// @return DD_RESULT_SUCCESS if the handling of the heartbeat was successful.
        static DD_RESULT IoWriteHeartbeat(void* userdata, DD_RESULT result, DD_IO_STATUS status, size_t bytes);

        /// @brief Reads from the current stream.
        /// @param [in] userdata A pointer to the trace source.
        /// @param [in] count The number of bytes to read.
        /// @param [in] buffer The buffer to read into.
        /// @param [in] bytes_read The number of bytes that were read (can be nullptr).
        /// @return 0 If the operation was successful, 1 if there was a failure.
        static int StreamRead(void* userdata, int64_t count, void* buffer, int64_t* bytes_read);

        /// @brief Writes to the current stream.
        /// @param [in] userdata A pointer to the trace source.
        /// @param [in] count The number of bytes to write.
        /// @param [in] buffer The buffer to write from.
        /// @param [in] bytes_written The number of bytes that were written (can be nullptr).
        /// @return 0 If the operation was successful, 1 if there was a failure.
        static int StreamWrite(void* userdata, int64_t count, const void* buffer, int64_t* bytes_written);

        /// @brief Provides the current position in the current stream.
        /// @param [in] userdata A pointer to the trace source.
        /// @param [in] position The place to write the position of the stream to.
        /// @return 0 if the operation was successful, 1 if there was a failure.
        static int StreamTell(void* userdata, int64_t* position);

        /// @brief Seeks to the position in the stream.
        /// @param [in] userdata A pointer to the trace source.
        /// @param [in] position The place to in the stream to seek to.
        /// @return If the operation was successful, 1 if there was a failure.
        static int StreamSeek(void* userdata, int64_t position);

        /// @brief Gets the total size of the stream.
        /// @param [in] userdata A pointer to the trace source.
        /// @param [in] size The place to write the size of the stream to.
        /// @return 0 if the operation was successful, 1 if there was a failure.
        static int StreamGetSize(void* userdata, int64_t* size);

        SourceWriterBase                 base_;             ///< Base file writer.
        std::unique_ptr<ReadWriteStream> stream_;           ///< The stream to write to.
        DDRdfFileWriter                  rdf_file_writer_;  ///< The RDF file writer;
        DDIOHeartbeat                    heartbeat_;        ///< The IO heartbeat for the file writer.
    };

}  // namespace devtrace

#endif
