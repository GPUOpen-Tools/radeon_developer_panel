// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the RGD chunk writer.

#include "rgd_chunk_writer.h"

#include <amdrdf.h>

#include "dev_trace_common.h"
#include "json/json_mapper.h"

namespace devtrace
{
    static constexpr auto kChunkIdentifier = "RgdExtendedInfo";

    bool Map(const DDEnhancedCrashInfoConfigFlags flags, JsonMapper mapper)
    {
        DEV_TRACE_ASSERT(mapper.IsSerializing());

        bool capture_wave_data    = flags.captureWaveData != 0;
        bool enable_single_mem_op = flags.enableSingleMemOp != 0;
        bool enable_single_alu_op = flags.enableSingleAluOp != 0;
        bool capture_sgpr_data    = flags.captureSGPRData;
        bool capture_vgpr_data    = flags.captureVGPRData;

        return mapper["captureWaveData"](capture_wave_data) && mapper["enableSingleMemOp"](enable_single_mem_op) &&
               mapper["enableSingleAluOp"](enable_single_alu_op) && mapper["captureSgprData"](capture_sgpr_data) &&
               mapper["captureVgprData"](capture_vgpr_data);
    }

    bool Map(ExtendedInfoChunkHardwareCrashInfoConfig config, JsonMapper& mapper)
    {
        return mapper["hcaEnabled"](config.hca_enabled) && Map(config.config.flags, mapper["hcaFlags"]);
    }

    bool Map(RgdSummaryOptions& options, JsonMapper& mapper)
    {
        return mapper["pdbSearchPaths"](options.pdb_search_paths) && mapper["pdbIncludeSubfolders"](options.pdb_include_subfolders);
    }

    DD_RESULT ExtendedInfoChunkWriter::Write(rdfChunkFileWriter* chunk_file_writer)
    {
        rdfChunkCreateInfo create_info{};
        create_info.headerSize = 0;
        create_info.pHeader    = nullptr;
        create_info.version    = 1;

        strcpy(create_info.identifier, kChunkIdentifier);

        JsonMapper mapper = JsonMapper::Serialize();

        if (hardware_crash_info_config_.has_value() && !Map(hardware_crash_info_config_.value(), mapper))
        {
            return DD_RESULT_UNKNOWN;
        }

        if (summary_options_.has_value() && !Map(summary_options_.value(), mapper))
        {
            return DD_RESULT_UNKNOWN;
        }

        const std::string data = mapper.ToString();

        int written_index = 0;

        if (const auto chunk_size = static_cast<int64_t>(data.size());
            rdfChunkFileWriterWriteChunk(chunk_file_writer, &create_info, chunk_size, data.c_str(), &written_index) != rdfResultOk)
        {
            return DD_RESULT_UNKNOWN;
        }

        return DD_RESULT_SUCCESS;
    }

    void ExtendedInfoChunkWriter::SetSummaryOptions(const RgdSummaryOptions& options)
    {
        summary_options_ = options;
    }

    void ExtendedInfoChunkWriter::SetEnhancedCrashConfig(const std::optional<ExtendedInfoChunkHardwareCrashInfoConfig>& config)
    {
        hardware_crash_info_config_ = config;
    }
}  // namespace devtrace
