// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the UberTrace client for crash analysis.

#ifndef RDP_SOURCE_TRACE_SRC_RGD_UBERTRACE_CLIENT_H_
#define RDP_SOURCE_TRACE_SRC_RGD_UBERTRACE_CLIENT_H_

#include <cstdint>
#include <string>

#include "../base_trace_source/ubertrace_client.h"
#include "rgd_trace_source.h"

namespace devtrace
{
    /// @brief Wrapper for client utils implemented somewhere else.
    class RgdUbertraceClientUtilsWrapper final : public ClientUtils<RgdTraceSourceConfig>
    {
    public:
        explicit RgdUbertraceClientUtilsWrapper(ClientUtils& impl);

        ~RgdUbertraceClientUtilsWrapper() override;

        void                           TraceCompleted(TraceCompletionStatus status, const std::string& path, DDConnectionId umd_connection_id) override;
        std::unique_ptr<ByteWriter>    GetByteWriter(std::string& path) override;
        std::unique_ptr<RdfWriter>     GetRdfWriter(std::string& path) override;
        const std::shared_ptr<Logger>& GetLogger() override;
        const RgdTraceSourceConfig&    GetClientConfig() override;

        /// @brief Gets the dumped data.
        /// @param [out] data The data to get.
        /// @return true if the data was the result of a successful trace, false otherwise.
        void GetData(std::vector<uint8_t>& data);

        void EmitPostProcessEvent([[maybe_unused]] const std::string& progress_text, [[maybe_unused]] float progress) override
        {
        }

        void ReportCaptureProgress([[maybe_unused]] const WritingProgress& progress) override
        {
        }

    private:
        ClientUtils&         impl_;  ///< The underlying implementation for the client utils.
        std::vector<uint8_t> data_;  ///< Data dumped from the byte writer.
    };

    /// @brief Accessory UberTrace client used for RGD.
    class RgdUbertraceClient final : public UbertraceClient<RgdTraceSourceConfig>, public ChunkWriter
    {
    public:
        /// @brief Constructor.
        /// @param [in] conn_info The connection information for the client.
        /// @param [in] client_utils_wrapper The wrapped utils that the client can use.
        /// @param [in] user The UberTrace user to use.
        RgdUbertraceClient(const ClientConnection&                         conn_info,
                           std::unique_ptr<RgdUbertraceClientUtilsWrapper> client_utils_wrapper,
                           std::unique_ptr<UbertraceUser>                  user);

        /// @brief Destructor.
        ~RgdUbertraceClient() override;

        DD_RESULT Write(rdfChunkFileWriter* chunk_file_writer) override;

    protected:
        void               GetPreliminarySources(std::vector<UberTraceSource>& sources) override;
        Result             GenerateCaptureConfig(const RgdTraceSourceConfig& config, UbertraceCaptureConfig& capture_config) override;
        [[nodiscard]] bool SupportsCaptureMode(uint32_t mode) const override;

    private:
        /// @brief Writes all the chunks using the chunk file writer.
        /// @param [in] chunk_file_writer The file writer to write chunks with.
        /// @param [in] file The file that was dumped from UberTrace.
        /// @param [in] iterator An iterator that iterates through the entire file.
        /// @return The result of the write operation.
        static DD_RESULT Write(rdfChunkFileWriter* chunk_file_writer, rdfChunkFile* file, rdfChunkFileIterator* iterator);

        std::unique_ptr<RgdUbertraceClientUtilsWrapper> client_utils_wrapper_;  ///< Client utils wrapper.
    };
}  // namespace devtrace

#endif
