// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for system info chunk writer.

#include "chunk_writing.h"

#include <string>

#include <dd_router_utils_api.h>

#include <system_info_writer.h>

#include "rgp_file_format.h"
#include "system_info_cache.h"
#include "trace_io.h"

namespace
{
    void* StdRealloc([[maybe_unused]] DDAllocatorInstance* instance, void* memory, [[maybe_unused]] size_t old_size, const size_t new_size)
    {
        return std::realloc(memory, new_size);
    }

    void StdFree([[maybe_unused]] DDAllocatorInstance* instance, void* memory, [[maybe_unused]] size_t size)
    {
        std::free(memory);
    }

    constexpr DDAllocator kDDAllocator = {nullptr, StdRealloc, StdFree};
}  // namespace

namespace devtrace
{
    AdditionalChunkWriter::AdditionalChunkWriter(const std::shared_ptr<SystemInfoCache>& system_info_cache, DDRouterUtilsApi* router_utils_api)
        : system_info_cache_(system_info_cache)
        , router_utils_api_(router_utils_api)
    {
    }

    ProcessInfoChunk AdditionalChunkWriter::PrepareProcessInfoChunk(const uint32_t pid) const
    {
        if (pid == 0)
        {
            return {};
        }

        char*  raw_process_path      = nullptr;
        size_t raw_process_path_size = 0;
        if (router_utils_api_->QueryPathByProcessId(router_utils_api_->pInstance, pid, kDDAllocator, &raw_process_path, &raw_process_path_size) !=
                DD_RESULT_SUCCESS ||
            raw_process_path == nullptr)
        {
            return {};
        }

        ProcessInfoChunk chunk{pid, raw_process_path};
        kDDAllocator.Free(kDDAllocator.pInstance, raw_process_path, raw_process_path_size);
        return chunk;
    }

    Result AdditionalChunkWriter::Write([[maybe_unused]] const DDConnectionId   umd_connection_id,
                                        const std::unique_ptr<ReadWriteStream>& stream,
                                        const ProcessInfoChunk&                 process_info,
                                        const ChunksToWrite                     chunks,
                                        const std::vector<ChunkWriter*>&        writers) const
    {
        std::string driver_settings = "";
        std::string sys_info        = "";
        std::string process_path    = "";

        auto result = Result::kSuccess;
        if ((chunks & kAdditionalChunkSystemInfo) != 0)
        {
            if (auto e_system_info = system_info_cache_->GetSystemInfoString(); e_system_info.has_value())
            {
                sys_info = std::move(*e_system_info);
            }
            else
            {
                result = Result::kFailure;
            }
        }

        if (result != Result::kSuccess)
        {
            return result;
        }

        ProcessInfoChunk process_info_to_write{};
        if (process_info.process_id != 0 && (chunks & kAdditionalChunkProcessInfo) != 0)
        {
            process_info_to_write = process_info;
        }

        return Write(sys_info, driver_settings, process_info_to_write, stream, writers);
    }

    Result AdditionalChunkWriter::Write(const std::string&                      sys_info,
                                        [[maybe_unused]] const std::string&     driver_settings,
                                        const ProcessInfoChunk&                 process_info,
                                        const std::unique_ptr<ReadWriteStream>& stream,
                                        const std::vector<ChunkWriter*>&        writers)
    {
        rdfUserStream user_stream;
        stream->GetRdfUserStream(user_stream);

        rdfStream* rdf_stream = nullptr;
        if (rdfStreamCreateFromUserStream(&user_stream, &rdf_stream) != rdfResultOk)
        {
            return Result::kFailure;
        }

        rdfChunkFileWriterCreateInfo writer_create_info{};
        writer_create_info.stream       = rdf_stream;
        writer_create_info.appendToFile = true;

        rdfChunkFileWriter* chunk_file_writer = nullptr;
        if (rdfChunkFileWriterCreate2(&writer_create_info, &chunk_file_writer) != rdfResultOk)
        {
            rdfStreamClose(&rdf_stream);
            return Result::kFailure;
        }

        DD_RESULT result = DD_RESULT_SUCCESS;
        if (!sys_info.empty())
        {
            result = system_info_utils::SystemInfoWriter::WriteRdfChunk(chunk_file_writer, sys_info);
        }

        if (result == DD_RESULT_SUCCESS && process_info.process_id != 0)
        {
            result = WriteProcessInfo(process_info, chunk_file_writer);
        }

        for (ChunkWriter* writer : writers)
        {
            if (result != DD_RESULT_SUCCESS)
            {
                continue;
            }

            result = writer->Write(chunk_file_writer);
        }

        rdfChunkFileWriterDestroy(&chunk_file_writer);
        rdfStreamClose(&rdf_stream);

        return result == DD_RESULT_SUCCESS ? Result::kSuccess : Result::kFailure;
    }

    DD_RESULT AdditionalChunkWriter::WriteProcessInfo(const ProcessInfoChunk& process_info, rdfChunkFileWriter* chunk_file_writer)
    {
        rdfChunkCreateInfo create_info{};
        create_info.version    = 1;
        create_info.headerSize = 0;
        create_info.pHeader    = nullptr;

        static const std::string chunk_id = "TraceProcessInfo";
        memcpy(create_info.identifier, chunk_id.c_str(), chunk_id.size());

        std::vector<uint8_t> byte_vector;
        const uint32_t       process_path_size = static_cast<uint32_t>(process_info.process_path.size() + 1);
        byte_vector.resize(sizeof(uint32_t) * 2 + process_path_size);

        const auto data = reinterpret_cast<uint32_t*>(byte_vector.data());
        *data           = process_info.process_id;
        *(data + 1)     = process_path_size;

        strcpy(reinterpret_cast<char*>(data + 2), process_info.process_path.c_str());

        int index = 0;
        if (rdfChunkFileWriterWriteChunk(chunk_file_writer, &create_info, byte_vector.size(), byte_vector.data(), &index) != rdfResultOk)
        {
            return DD_RESULT_DD_GENERIC_UNKNOWN;
        }

        return DD_RESULT_SUCCESS;
    }

    Result AdditionalChunkWriter::WriteLegacyRgp([[maybe_unused]] const DDConnectionId                    umd_connection_id,
                                                 [[maybe_unused]] const std::unique_ptr<ReadWriteStream>& stream) const
    {
        return Result::kSuccess;
    }

    ChunkWriterReservationId AdditionalChunkWriter::Reserve([[maybe_unused]] const DDConnectionId umd_connection_id) const
    {
        return kInvalidChunkWriterReservationId;
    }

    void AdditionalChunkWriter::ReleaseReservation([[maybe_unused]] const DDConnectionId           umd_connection_id,
                                                   [[maybe_unused]] const ChunkWriterReservationId reservation) const
    {
    }
}  // namespace devtrace
