// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the UberTrace client for crash analysis.

#include "rgd_ubertrace_client.h"

#include "rgd_byte_writer.h"

namespace devtrace
{
    RgdUbertraceClientUtilsWrapper::RgdUbertraceClientUtilsWrapper(ClientUtils& impl)
        : impl_(impl)
    {
    }

    RgdUbertraceClientUtilsWrapper::~RgdUbertraceClientUtilsWrapper() = default;

    void RgdUbertraceClientUtilsWrapper::TraceCompleted([[maybe_unused]] TraceCompletionStatus status,
                                                        [[maybe_unused]] const std::string&    path,
                                                        [[maybe_unused]] DDConnectionId        umd_connection_id)
    {
    }

    std::unique_ptr<ByteWriter> RgdUbertraceClientUtilsWrapper::GetByteWriter(std::string& path)
    {
        data_.clear();
        path = "";

        return std::make_unique<RgdByteWriter>(data_);
    }

    std::unique_ptr<RdfWriter> RgdUbertraceClientUtilsWrapper::GetRdfWriter(std::string& path)
    {
        // This should never be called
        DEV_TRACE_ASSERT(false);
        // ReSharper disable once CppDFAUnreachableCode
        return impl_.GetRdfWriter(path);
    }

    const std::shared_ptr<Logger>& RgdUbertraceClientUtilsWrapper::GetLogger()
    {
        return impl_.GetLogger();
    }

    const RgdTraceSourceConfig& RgdUbertraceClientUtilsWrapper::GetClientConfig()
    {
        return impl_.GetClientConfig();
    }

    void RgdUbertraceClientUtilsWrapper::GetData(std::vector<uint8_t>& data)
    {
        data = std::move(data_);
    }

    RgdUbertraceClient::RgdUbertraceClient(const ClientConnection&                         conn_info,
                                           std::unique_ptr<RgdUbertraceClientUtilsWrapper> client_utils_wrapper,
                                           std::unique_ptr<UbertraceUser>                  user)
        : UbertraceClient(conn_info, *client_utils_wrapper, std::move(user), nullptr, true)
        , client_utils_wrapper_(std::move(client_utils_wrapper))
    {
    }

    RgdUbertraceClient::~RgdUbertraceClient() = default;

    void RgdUbertraceClient::GetPreliminarySources(std::vector<UberTraceSource>& sources)
    {
        UberTraceSource code_object{};
        code_object.name   = "codeobject";
        code_object.config = std::make_shared<UbertraceSourceConfig>();

        sources.push_back(code_object);
    }

    Result RgdUbertraceClient::GenerateCaptureConfig([[maybe_unused]] const RgdTraceSourceConfig& config, UbertraceCaptureConfig& capture_config)
    {
        if (!SupportsCaptureMode(capture_config.capture_mode))
        {
            return Result::kUnsupported;
        }

        UbertraceController tdr_controller{};
        tdr_controller.name   = "tdr";
        tdr_controller.config = std::make_shared<UbertraceControllerConfig>(true);

        capture_config.ubertrace_config.controllers = std::make_unique<UbertraceControllerCollection>(GetFeatures(), tdr_controller);
        GetPreliminarySources(capture_config.ubertrace_config.sources);

        return Result::kSuccess;
    }

    bool RgdUbertraceClient::SupportsCaptureMode(const uint32_t mode) const
    {
        return mode == 0;
    }

    DD_RESULT RgdUbertraceClient::Write(rdfChunkFileWriter* chunk_file_writer)
    {
        std::vector<uint8_t> data;
        client_utils_wrapper_->GetData(data);

        const auto data_size = static_cast<int64_t>(data.size());
        rdfStream* memory_stream;
        if (rdfStreamFromReadOnlyMemory(data_size, data.data(), &memory_stream) != rdfResultOk)
        {
            return DD_RESULT_UNKNOWN;
        }

        rdfChunkFile* file;
        if (rdfChunkFileOpenStream(memory_stream, &file) != rdfResultOk)
        {
            rdfStreamClose(&memory_stream);
            return DD_RESULT_UNKNOWN;
        }

        rdfChunkFileIterator* iterator;
        if (rdfChunkFileCreateChunkIterator(file, &iterator) != rdfResultOk)
        {
            rdfChunkFileClose(&file);
            rdfStreamClose(&memory_stream);
        }

        const DD_RESULT result = Write(chunk_file_writer, file, iterator);

        rdfChunkFileDestroyChunkIterator(&iterator);
        rdfChunkFileClose(&file);
        rdfStreamClose(&memory_stream);

        return result;
    }

    DD_RESULT RgdUbertraceClient::Write(rdfChunkFileWriter* chunk_file_writer, rdfChunkFile* file, rdfChunkFileIterator* iterator)
    {
        int at_end = 0;
        while (rdfChunkFileIteratorIsAtEnd(iterator, &at_end) == rdfResultOk && at_end == 0)
        {
            rdfChunkCreateInfo create_info{};
            if (rdfChunkFileIteratorGetChunkIdentifier(iterator, create_info.identifier) != rdfResultOk)
            {
                return DD_RESULT_UNKNOWN;
            }

            int index;
            if (rdfChunkFileIteratorGetChunkIndex(iterator, &index) != rdfResultOk)
            {
                return DD_RESULT_UNKNOWN;
            }

            int64_t data_size;
            if (rdfChunkFileGetChunkDataSize(file, create_info.identifier, index, &data_size) != rdfResultOk)
            {
                return DD_RESULT_UNKNOWN;
            }

            std::vector<uint8_t> data(data_size);
            if (rdfChunkFileReadChunkData(file, create_info.identifier, index, data.data()) != rdfResultOk)
            {
                return DD_RESULT_UNKNOWN;
            }

            if (rdfChunkFileGetChunkHeaderSize(file, create_info.identifier, index, &create_info.headerSize) != rdfResultOk)
            {
                return DD_RESULT_UNKNOWN;
            }

            std::vector<uint8_t> header_data(data_size);
            create_info.pHeader = header_data.data();

            if (rdfChunkFileReadChunkHeader(file, create_info.identifier, index, header_data.data()) != rdfResultOk)
            {
                return DD_RESULT_UNKNOWN;
            }

            if (rdfChunkFileGetChunkVersion(file, create_info.identifier, index, &create_info.version) != rdfResultOk)
            {
                return DD_RESULT_UNKNOWN;
            }

            int written_index = 0;
            if (rdfChunkFileWriterWriteChunk(chunk_file_writer, &create_info, data_size, data.data(), &written_index) != rdfResultOk)
            {
                return DD_RESULT_UNKNOWN;
            }

            if (rdfChunkFileIteratorAdvance(iterator) != rdfResultOk)
            {
                return DD_RESULT_UNKNOWN;
            }
        }

        return DD_RESULT_SUCCESS;
    }
}  // namespace devtrace
