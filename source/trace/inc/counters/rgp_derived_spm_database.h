// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  RGP file DerivedSpmDb chunk handling.

#ifndef RDP_SOURCE_TRACE_SRC_COUNTERS_RGP_DERIVED_SPM_DATABASE_H_
#define RDP_SOURCE_TRACE_SRC_COUNTERS_RGP_DERIVED_SPM_DATABASE_H_

#include <atomic>
#include <string>
#include <vector>

#include <rgp_trace_source.h>
#include "spm_db/derived_spm_database.h"

#include <logging.h>

namespace devtrace
{
    class ReadWriteStream;
}

using DerivedSpmCounter = devtrace::DerivedSpmCounter;
using DerivedSpmGroup   = devtrace::DerivedSpmGroup;

struct DerivedSpmDbArguments
{
    std::unique_ptr<devtrace::ReadWriteStream> stream;          ///< The stream to append to.
    bool                                       is_rdf = false;  ///< true if the stream is using RDF.

    std::vector<DerivedSpmCounter> counters;                    ///< A list of derived counters to compute for the trace.
    std::vector<DerivedSpmGroup>   groups;                      ///< A list of derived spm groups to define in the trace.
    bool                           filter_bad_spm_data = true;  ///< Whether we should exclude SPM counters with negative values.

    const std::atomic_bool*    should_abort;       ///< Will be set to true if the processing should be aborted.
    std::function<void(float)> progress_callback;  ///< An optional callback that's used to emit processing progress.

    std::shared_ptr<devtrace::Logger> logger;  ///< The logger to use.
};

typedef enum RgpFileWriterStatus
{
    kRgpFileWriterStatusOk             = 0,  ///< No problems encountered.
    kRgpFileWriterStatusFileReadError  = 1,  ///< File read error encountered.
    kRgpFileWriterStatusParserError    = 2,  ///< Parser error encountered.
    kRgpFileWriterStatusMemoryError    = 3,  ///< Out of memory read error encountered.
    kRgpFileWriterStatusFileWriteError = 4,  ///< File write error encountered.
    kRgpFileWriterStatusSpmDbError     = 5,  ///< SpmDb error encountered.
    kRgpFileWriterStatusAborted        = 6,  ///< SPM was aborted.
} RgpFileWriterStatus;

/// @brief Appends a DerivedSpmDb chunk to a .rgp file.
///
/// @param [in] logger Handle to the log wrapper.
/// @param [in] args Arguments for appending the DerivedSpmDb chunk.
///
/// @retval kRgpFileWriterStatusOk             The operation completed successfully.
/// @retval kRgpFileWriterStatusFileReadError  There was an error reading the file at filepath.
/// @retval kRgpFileWriterStatusParserError    There was an error parsing the .rgp file at filepath.
/// @retval kRgpFileWriterStatusFileWriteError There was an error writing to the file at filepath.
/// @retval kRgpFileWriterStatusSpmDbError     There was an error with the initializing the SPM database.
RgpFileWriterStatus AppendChunkDerivedSpmDb(const DerivedSpmDbArguments& args);

/// @brief Appends the derived SPM data to the rdf file.
///
/// @param [in] derived_spm_db The derived SPM DB to serialize.
/// @param [in] chunk_file_writer The chunk file to write to.
///
/// @return The status of writing the chunk.
RgpFileWriterStatus AppendChunks(const spm_db::DerivedSpmDataBase& derived_spm_db, rdfChunkFileWriter* chunk_file_writer);

#endif
