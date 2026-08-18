// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for additional chunk writer.

#ifndef RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_CHUNK_WRITING_H_
#define RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_CHUNK_WRITING_H_

#include <memory>
#include <string>

#include <dd_common_api.h>

#include "dev_trace_common.h"
#include "dipper.h"
#include "trace_io.h"

struct DDRouterUtilsApi;

namespace devtrace
{
    /// @brief The chunks that can be written.
    enum AdditionalChunks : uint8_t
    {
        kAdditionalChunkSystemInfo           = 0b001,
        kAdditionalChunkDriverSettingsValues = 0b010,
        kAdditionalChunkProcessInfo          = 0b100,
        kAdditionalChunkAll                  = 0xFF
    };

    using ChunksToWrite = uint8_t;  ///< Bitmask of chunks to write.

    /// @brief Chunk that contains process info.
    struct ProcessInfoChunk
    {
        uint32_t    process_id;    ///< PID of the process.
        std::string process_path;  ///< Absolute path of the process.
    };

    using ChunkWriterReservationId = uint64_t;  ///< Chunk writing reservations.

    /// @brief Invalid reservation id.
    static constexpr ChunkWriterReservationId kInvalidChunkWriterReservationId = static_cast<ChunkWriterReservationId>(-1);

    /// @brief Interface that writes chunk(s).
    class ChunkWriter
    {
    public:
        virtual ~ChunkWriter() = default;

        /// @brief Writes the chunk(s)
        /// @param [in] chunk_file_writer The RDF file writer to use.
        /// @return The result of the writing operating.
        virtual DD_RESULT Write(rdfChunkFileWriter* chunk_file_writer) = 0;
    };

    /// @brief Writes additional chunks to a stream.
    class AdditionalChunkWriter
    {
    public:
        /// @brief Constructor.
        /// @param [in] system_info_cache The object to use to query the system info.
        /// @param [in] router_utils_api The router utils to query PID for.
        DIP(AdditionalChunkWriter(const std::shared_ptr<class SystemInfoCache>& system_info_cache, DDRouterUtilsApi* router_utils_api));

        /// @brief Prepares the process info chunk for writing later since the process must be alive to get this information.
        /// @param [in] pid The PID of the process.
        ProcessInfoChunk PrepareProcessInfoChunk(uint32_t pid) const;

        /// @brief Writes the chunks to an RDF file already open in the stream.
        /// @param [in] umd_connection_id The connection that the writing is happening for.
        /// @param [in] stream The stream to write the system info chunk into.
        /// @param [in] chunks The chunks to write.
        /// @param [in] process_info The process info chunk.
        /// @param [in] writers Objects that will write even more chunks.
        /// @return The result of the writing operation.
        Result Write(DDConnectionId                          umd_connection_id,
                     const std::unique_ptr<ReadWriteStream>& stream,
                     const ProcessInfoChunk&                 process_info,
                     ChunksToWrite                           chunks  = kAdditionalChunkAll,
                     const std::vector<ChunkWriter*>&        writers = {}) const;

    private:
        /// @brief Writes the system info to an RDF file already open in the stream.
        /// @param [in] sys_info The system info string to write.
        /// @param [in] driver_settings The modified driver settings to write.
        /// @param [in] process_info The process info chunk.
        /// @param [in] stream The stream to write the system info chunk into.
        /// @param [in] writers Objects that will write even more chunks.
        /// @return The result of the writing operation.
        static Result Write(const std::string&                      sys_info,
                            const std::string&                      driver_settings,
                            const ProcessInfoChunk&                 process_info,
                            const std::unique_ptr<ReadWriteStream>& stream,
                            const std::vector<ChunkWriter*>&        writers);

        /// @brief Writes the process info chunk.
        /// @param [in] process_info The process info chunk.
        /// @param [in] chunk_file_writer The chunk file writer to use.
        /// @return The result of writing the driver settings chunk.
        static DD_RESULT WriteProcessInfo(const ProcessInfoChunk& process_info, rdfChunkFileWriter* chunk_file_writer);

    public:
        /// @brief Writes the additional legacy RGP chunks (driver settings).
        /// @param [in] umd_connection_id The connection that the writing is happening for.
        /// @param [in] stream The stream to write the system info chunk into.
        /// @return The result of the writing operation.
        Result WriteLegacyRgp(DDConnectionId umd_connection_id, const std::unique_ptr<ReadWriteStream>& stream) const;

        /// @brief Reserves the data needed for the chunks for the connection.
        /// @param [in] umd_connection_id The connection to reserve driver settings for.
        /// @return The id of the reservation.
        ChunkWriterReservationId Reserve(DDConnectionId umd_connection_id) const;

        /// @brief Releases the reservation for the connection.
        /// @param [in] umd_connection_id The connection to release the reservation for.
        /// @param [in] reservation The reservation to release.
        void ReleaseReservation(DDConnectionId umd_connection_id, ChunkWriterReservationId reservation) const;

    private:
        std::shared_ptr<SystemInfoCache> system_info_cache_;  ///< The object to use to query the system info.
        DDRouterUtilsApi*                router_utils_api_;   ///< The router utils API.
    };
}  // namespace devtrace

#endif
