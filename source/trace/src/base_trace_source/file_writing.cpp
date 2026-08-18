// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for file writing utilities.

#include "file_writing.h"

#include "dev_trace_common.h"

namespace devtrace
{
    WriterBase::WriterBase()  = default;
    WriterBase::~WriterBase() = default;

    void WriterBase::RegisterWritingStatusEvent(const WritingStatusEvent& event)
    {
        std::lock_guard lock(status_events_mutex_);
        status_events_.emplace_back(event);
    }

    void WriterBase::EmitWritingStatusEvent(const WritingStatusEventArgs& args)
    {
        std::lock_guard lock(status_events_mutex_);
        for (auto& [listener, callback] : status_events_)
        {
            if (callback)
            {
                callback(listener, args);
            }
        }
    }

    SourceWriterBase::SourceWriterBase(std::mutex& writing_mutex, const std::function<void(const WritingProgress&)>& report_progress)
        : writing_mutex_(writing_mutex)
        , report_progress_(report_progress)
    {
    }

    void SourceWriterBase::SetPostProcessOperation(const std::function<void(const std::unique_ptr<ReadWriteStream>&)>& operation)
    {
        postprocess_ = operation;
    }

    WritingProgress& SourceWriterBase::GetCurrentProgress()
    {
        return current_progress_;
    }

    void SourceWriterBase::ReportCurrentProgress() const
    {
        if (report_progress_)
        {
            report_progress_(current_progress_);
        }
    }

    void SourceWriterBase::BeginWriting()
    {
        //writing_lock_ = std::unique_lock<std::mutex>(writing_mutex_);

        current_progress_ = {};

        WritingStatusEventArgs args{};
        args.is_writing = true;
        args.progress   = current_progress_;
        EmitWritingStatusEvent(args);
    }

    void SourceWriterBase::PostProcess(const std::unique_ptr<ReadWriteStream>& stream) const
    {
        if (postprocess_)
        {
            postprocess_(stream);
        }
    }

    SourceByteWriter::SourceByteWriter(std::unique_ptr<ReadWriteStream>&                  stream,
                                       std::mutex&                                        writing_mutex,
                                       const std::function<void(const WritingProgress&)>& report_progress)
        : base_(writing_mutex, report_progress)
        , stream_(std::move(stream))
    {
        writer_.pUserdata     = this;
        writer_.pfnBegin      = ByteWriterBegin;
        writer_.pfnWriteBytes = ByteWriterWriteBytes;
        writer_.pfnEnd        = ByteWriterEnd;
    }

    void SourceByteWriter::RegisterWritingStatusEvent(const WritingStatusEvent& event)
    {
        base_.RegisterWritingStatusEvent(event);
    }

    const DDByteWriter& SourceByteWriter::Writer()
    {
        return writer_;
    }

    void SourceByteWriter::SetPostProcessOperation(const std::function<void(const std::unique_ptr<ReadWriteStream>& stream)>& operation)
    {
        base_.SetPostProcessOperation(operation);
    }

    DD_RESULT SourceByteWriter::ByteWriterBegin(void* userdata, const size_t* total_data_size)
    {
        const auto writer = static_cast<SourceByteWriter*>(userdata);
        writer->base_.BeginWriting();

        if (writer->stream_ == nullptr || !writer->stream_->Open())
        {
            return DD_RESULT_UNKNOWN;
        }

        writer->base_.GetCurrentProgress().total_bytes_to_dump = total_data_size != nullptr ? static_cast<uint32_t>(*total_data_size) : 0;
        writer->base_.ReportCurrentProgress();

        return DD_RESULT_SUCCESS;
    }

    DD_RESULT SourceByteWriter::ByteWriterWriteBytes(void* userdata, const void* data, const size_t data_size)
    {
        DEV_TRACE_ASSERT(data != nullptr);
        DEV_TRACE_ASSERT(data_size != 0);

        const auto writer = static_cast<SourceByteWriter*>(userdata);
        if (writer->stream_ == nullptr)
        {
            return DD_RESULT_UNKNOWN;
        }

        std::int64_t written_bytes;
        if (const std::int64_t data_size_u = static_cast<std::int64_t>(data_size);
            !writer->stream_->Write(data_size_u, data, &written_bytes) || written_bytes != data_size_u)
        {
            return DD_RESULT_UNKNOWN;
        }

        auto& [total_bytes_to_dump, num_bytes_dumped, progress] = writer->base_.GetCurrentProgress();
        num_bytes_dumped += static_cast<uint64_t>(data_size);
        total_bytes_to_dump = std::max(total_bytes_to_dump, num_bytes_dumped);
        progress            = static_cast<float>(num_bytes_dumped) / static_cast<float>(total_bytes_to_dump);

        writer->base_.ReportCurrentProgress();

        return DD_RESULT_SUCCESS;
    }

    void SourceByteWriter::ByteWriterEnd(void* userdata, [[maybe_unused]] DD_RESULT result)
    {
        const auto writer = static_cast<SourceByteWriter*>(userdata);
        if (writer->stream_ == nullptr)
        {
            return;
        }

        writer->base_.PostProcess(writer->stream_);
        writer->stream_->Close();
    }

    SourceRdfWriter::SourceRdfWriter(std::unique_ptr<ReadWriteStream>&                  stream,
                                     std::mutex&                                        writing_mutex,
                                     const std::function<void(const WritingProgress&)>& report_progress)
        : base_(writing_mutex, report_progress)
        , stream_(std::move(stream))
    {
        heartbeat_.pfnWriteHeartbeat = IoWriteHeartbeat;
        heartbeat_.pUserdata         = this;

        rdf_file_writer_.pfnFileRead    = StreamRead;
        rdf_file_writer_.pfnFileWrite   = StreamWrite;
        rdf_file_writer_.pfnFileTell    = StreamTell;
        rdf_file_writer_.pfnFileSeek    = StreamSeek;
        rdf_file_writer_.pfnFileGetSize = StreamGetSize;
        rdf_file_writer_.pUserData      = this;
    }

    const DDRdfFileWriter& SourceRdfWriter::Writer()
    {
        return rdf_file_writer_;
    }

    const DDIOHeartbeat& SourceRdfWriter::Heartbeat()
    {
        return heartbeat_;
    }

    void SourceRdfWriter::SetPostProcessOperation(const std::function<void(const std::unique_ptr<ReadWriteStream>& stream)>& operation)
    {
        base_.SetPostProcessOperation(operation);
    }

    // ReSharper disable once CppDFAUnreachableFunctionCall
    DD_RESULT SourceRdfWriter::HandleIoBegin(const DD_RESULT result, const size_t total_bytes)
    {
        base_.BeginWriting();

        if (result != DD_RESULT_SUCCESS)
        {
            return result;
        }

        if (stream_ == nullptr || !stream_->Open())
        {
            return DD_RESULT_UNKNOWN;
        }

        base_.GetCurrentProgress().total_bytes_to_dump = static_cast<uint32_t>(total_bytes);
        base_.ReportCurrentProgress();

        return DD_RESULT_SUCCESS;
    }

    // ReSharper disable once CppDFAUnreachableFunctionCall
    DD_RESULT SourceRdfWriter::HandleIoWrite(const DD_RESULT result, const size_t bytes)
    {
        if (result != DD_RESULT_SUCCESS)
        {
            return result;
        }

        auto& [total_bytes_to_dump, num_bytes_dumped, progress] = base_.GetCurrentProgress();
        num_bytes_dumped += static_cast<uint64_t>(bytes);

        // The total bytes passed during the begin() call was only an estimate. If more
        // bytes have been written than the estimate, then the total needs to be updated
        total_bytes_to_dump = std::max(total_bytes_to_dump, num_bytes_dumped);
        progress            = static_cast<float>(num_bytes_dumped) / static_cast<float>(total_bytes_to_dump);

        base_.ReportCurrentProgress();

        return DD_RESULT_SUCCESS;
    }

    // ReSharper disable once CppDFAUnreachableFunctionCall
    DD_RESULT SourceRdfWriter::HandleIoEnd(const DD_RESULT result) const
    {
        if (stream_ == nullptr)
        {
            return result;
        }

        base_.PostProcess(stream_);

        stream_->Close();
        return result;
    }

    DD_RESULT SourceRdfWriter::IoWriteHeartbeat(void* userdata, const DD_RESULT result, const DD_IO_STATUS status, const size_t bytes)
    {
        const auto writer = static_cast<SourceRdfWriter*>(userdata);
        switch (status)
        {
        case DD_IO_STATUS_BEGIN:
            return writer->HandleIoBegin(result, bytes);
        case DD_IO_STATUS_WRITE:
            return writer->HandleIoWrite(result, bytes);
        case DD_IO_STATUS_END:
            return writer->HandleIoEnd(result);
        default:
            return DD_RESULT_UNKNOWN;
        }
    }

    int SourceRdfWriter::StreamRead(void* userdata, const int64_t count, void* buffer, int64_t* bytes_read)
    {
        const auto writer = static_cast<SourceRdfWriter*>(userdata);
        return !writer->stream_->Read(count, buffer, bytes_read);
    }

    int SourceRdfWriter::StreamWrite(void* userdata, const int64_t count, const void* buffer, int64_t* bytes_written)
    {
        const auto writer = static_cast<SourceRdfWriter*>(userdata);
        return !writer->stream_->Write(count, buffer, bytes_written);
    }

    int SourceRdfWriter::StreamTell(void* userdata, int64_t* position)
    {
        const auto writer = static_cast<SourceRdfWriter*>(userdata);
        return !writer->stream_->Tell(position);
    }

    int SourceRdfWriter::StreamSeek(void* userdata, const int64_t position)
    {
        const auto writer = static_cast<SourceRdfWriter*>(userdata);
        return !writer->stream_->Seek(position);
    }

    int SourceRdfWriter::StreamGetSize(void* userdata, int64_t* size)
    {
        const auto writer = static_cast<SourceRdfWriter*>(userdata);
        return !writer->stream_->GetSize(size);
    }
}  // namespace devtrace
