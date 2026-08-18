// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the RGD chunk writer.

#ifndef RDP_SOURCE_TRACE_SRC_RGD_RGD_CHUNK_WRITER_H_
#define RDP_SOURCE_TRACE_SRC_RGD_RGD_CHUNK_WRITER_H_

#include <optional>

#include <dd_enhanced_crash_info_api.h>

#include "rgd_summary_options.h"

#include "chunk_writing.h"

namespace devtrace
{
    struct ExtendedInfoChunkHardwareCrashInfoConfig
    {
        bool                      hca_enabled = false;
        DDEnhancedCrashInfoConfig config;
    };

    /// @brief Chunk writer for extended crash info chunk.
    class ExtendedInfoChunkWriter final : public ChunkWriter
    {
    public:
        DD_RESULT Write(rdfChunkFileWriter* chunk_file_writer) override;

        /// @brief Sets the enhanced crash info config to write.
        /// @param [in] config The enhanced crash info config.
        void SetEnhancedCrashConfig(const std::optional<ExtendedInfoChunkHardwareCrashInfoConfig>& config);

        /// @brief Sets the summary options to write.
        /// @param [in] options The summary options.
        void SetSummaryOptions(const RgdSummaryOptions& options);

    private:
        std::optional<RgdSummaryOptions>                        summary_options_{};             ///< The hardware crash info config summary options.
        std::optional<ExtendedInfoChunkHardwareCrashInfoConfig> hardware_crash_info_config_{};  ///< The hardware crash info config.
    };
}  // namespace devtrace

#endif
