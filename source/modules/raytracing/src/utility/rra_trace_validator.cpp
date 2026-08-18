// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for a validator to make sure that an RRA trace has BVH data.

#include "rra_trace_validator.h"

#include "common/inc/model/qt_trace_io.h"

// This chunk was renamed, so both are checked for compatibility with older drivers
static constexpr auto kAccelStructNameOld     = "RawAccelStruc";
static constexpr auto kAccelStructNameNew     = "RawAccelStruct";
static constexpr auto kRayHistoryName         = "HistoryTokensRaw";
static constexpr auto kRayHistoryMetadataName = "HistoryMetadata";

static constexpr size_t kRayHistoryMetadataLostBytesIndex = 16;

bool HasMissingBytesInDispatches(rdfChunkFile* chunk_file)
{
    int64_t metadata_chunk_count = 0;
    rdfChunkFileGetChunkCount(chunk_file, kRayHistoryMetadataName, &metadata_chunk_count);

    for (auto i = 0; std::cmp_less(i, metadata_chunk_count); ++i)
    {
        int64_t metadata_chunk_size = 0;
        rdfChunkFileGetChunkDataSize(chunk_file, kRayHistoryMetadataName, i, &metadata_chunk_size);

        std::vector<uint32_t> buffer(metadata_chunk_size / 4);
        rdfChunkFileReadChunkData(chunk_file, kRayHistoryMetadataName, i, buffer.data());

        if (buffer[kRayHistoryMetadataLostBytesIndex] > 0)
        {
            // We have lost some bytes during capture.
            return true;
        }
    }

    return false;
}

RraTraceValidationResult RraTraceValidator::ValidateFile(const QString& file_path, const bool validate_ray_history)
{
    // In cases where there is a failure, we return true so that the file doesn't
    // get deleted.
    QFileStream stream(file_path);
    stream.SetOpenMode(QIODevice::ReadWrite);

    if (!stream.Open())
    {
        return RraTraceValidationResult::kFailedToValidate;
    }

    rdfUserStream user_stream{};
    stream.GetRdfUserStream(user_stream);

    rdfStream* rdf_stream = nullptr;
    if (rdfStreamCreateFromUserStream(&user_stream, &rdf_stream) != rdfResultOk)
    {
        stream.Close();
        return RraTraceValidationResult::kFailedToValidate;
    }

    rdfChunkFile* chunk_file = nullptr;
    if (rdfChunkFileOpenStream(rdf_stream, &chunk_file) != rdfResultOk)
    {
        rdfStreamClose(&rdf_stream);

        return RraTraceValidationResult::kFailedToValidate;
    }

    std::int64_t old_chunk_count = 0;
    std::int64_t new_chunk_count = 0;

    bool failure = rdfChunkFileGetChunkCount(chunk_file, kAccelStructNameOld, &old_chunk_count) != rdfResultOk ||
                   rdfChunkFileGetChunkCount(chunk_file, kAccelStructNameNew, &new_chunk_count) != rdfResultOk;

    bool ray_history_valid = true;
    if (validate_ray_history)
    {
        std::int64_t ray_history_chunk_count = 0;
        failure |= rdfChunkFileGetChunkCount(chunk_file, kRayHistoryName, &ray_history_chunk_count) != rdfResultOk;

        ray_history_valid = ray_history_chunk_count != 0;
    }

    const bool has_incomplete_dispatch_data = HasMissingBytesInDispatches(chunk_file);

    rdfChunkFileClose(&chunk_file);
    rdfStreamClose(&rdf_stream);

    if (failure)
    {
        return RraTraceValidationResult::kFailedToValidate;
    }

    if (old_chunk_count == 0 && new_chunk_count == 0)
    {
        return RraTraceValidationResult::kMissingBvh;
    }

    if (has_incomplete_dispatch_data)
    {
        return RraTraceValidationResult::kIncompleteRayHistory;
    }

    return ray_history_valid ? RraTraceValidationResult::kSuccess : RraTraceValidationResult::kMissingRayHistory;
}
