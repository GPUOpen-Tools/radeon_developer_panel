// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  RGP file validator.
///
///         Most of this file is copied/pasted from the RGP backend.

#include <QFile>
#include <QString>

#include "rgp_file_validator.h"

#include <amdrdf.h>
#include <rgp_file_format.h>

// RDF chunk identifier strings for chunks that RGP requires.
static constexpr const char* kRgpRdfAsicInfoId         = "AsicInfo";
static constexpr const char* kRgpRdfApiInfoId          = "ApiInfo";
static constexpr const char* kRgpRdfTraceConfigId      = "TraceConfig";
static constexpr const char* kRgpRdfSqttDataId         = "SqttData";
static constexpr const char* kRgpRdfQueueInfoId        = "QueueInfo";
static constexpr const char* kRgpRdfQueueEventId       = "QueueEvent";
static constexpr const char* kRgpRdfClockCalibrationId = "ClockCalibration";
static constexpr const char* kRgpRdfSpmSessionId       = "SpmSession";

/// @brief A structure encapsulating the state of the RGP file parser.
typedef struct RgpFileParser
{
    RgpFileHeader header;             ///< The RGP file header read from the buffer.
    int32_t       next_chunk_offset;  ///< The offset to the next chunk to read.
    const void*   file_buffer;        ///< A pointer to the file buffer to parse.
    size_t        file_buffer_size;   ///< The size (in bytes) of the file buffer to parse.
} RgpFileParser;

/// @brief Create an RGP file parser from a buffer.
///
/// @param [in,out] file_parser      A pointer to a <c><i>RgpFileParser</i></c> structure to populate.
/// @param [in]     file_buffer      A pointer to a buffer in memory containing the RGP file data.
/// @param [in]     file_buffer_size The size (specified in bytes) of the buffer pointed to by <c><i>file_buffer</i></c>.
///
/// @retval
/// kRgpOk                  The operation completed successfully.
/// @retval
/// kRgpErrorInvalidPointer The operation failed because <c><i>file_parser</i></c> or <c><i>file_buffer</i></c> was NULL.
/// @retval
/// kRgpErrorInvalidSize    The operation failed because <c><i>file_buffer_size</i></c> was not large enough to contain the RGP file header.
/// @retval
/// kRgpErrorMalformedData  The operation failed because the data pointed to by <c><i>file_buffer</i></c> didn't begin with a valid RGP file header.
bool RgpFileParserCreateFromBuffer(RgpFileParser* file_parser, const void* file_buffer, size_t file_buffer_size)
{
    if (file_parser == nullptr || file_buffer == nullptr || file_buffer_size < sizeof(RgpFileHeader))
    {
        return false;
    }

    // Set up the parser.
    file_parser->file_buffer       = file_buffer;
    file_parser->file_buffer_size  = file_buffer_size;
    file_parser->next_chunk_offset = 0;

    // Now parse the header.
    const uint32_t* current_file_ptr        = (const uint32_t*)file_buffer;
    file_parser->header.magic_number        = *current_file_ptr++;
    file_parser->header.version_major       = *current_file_ptr++;
    file_parser->header.version_minor       = *current_file_ptr++;
    file_parser->header.flags               = *current_file_ptr++;
    file_parser->header.chunk_offset        = *current_file_ptr++;
    file_parser->header.second              = *current_file_ptr++;
    file_parser->header.minute              = *current_file_ptr++;
    file_parser->header.hour                = *current_file_ptr++;
    file_parser->header.day_in_month        = *current_file_ptr++;
    file_parser->header.month               = *current_file_ptr++;
    file_parser->header.year                = *current_file_ptr++;
    file_parser->header.day_in_week         = *current_file_ptr++;
    file_parser->header.day_in_year         = *current_file_ptr++;
    file_parser->header.is_daylight_savings = *current_file_ptr++;

    // Set the next chunk offset.
    file_parser->next_chunk_offset = file_parser->header.chunk_offset;

    // Validate that the file contains the magic number.
    if (file_parser->header.magic_number != kRgpFileMagicNumber)
    {
        return false;
    }

    return true;
}

/// @brief Get a pointer to the next chunk in the file.
///
/// @param [in,out] file_parser  A pointer to a <c><i>RgpFileParser</i></c> structure.
/// @param [out]    parsed_chunk A pointer to a <c><i>RgpFileChunkHeader</i></c> structure to populate with the chunk information.
///
/// @retval
/// kRgpOk                  The operation completed successfully.
/// @retval
/// kRgpErrorInvalidPointer The operation failed because <c><i>fileParser</i></c> or <c><i>parsedChunk</i></c> was NULL.
/// @retval
/// kRgpEndOfFile           The operation failed because the end of the buffer was reached.
bool RgpFileParserParseNextChunk(RgpFileParser* file_parser, RgpFileChunkHeader** parsed_chunk)
{
    if (file_parser == nullptr || parsed_chunk == nullptr)
    {
        return false;
    }

    *parsed_chunk = nullptr;

    // Check the chunk is within the file buffer.
    if (file_parser->next_chunk_offset >= static_cast<int32_t>(file_parser->file_buffer_size))
    {
        return false;
    }

    // Work out the read pointer for this chunk.
    const uintptr_t buffer_address   = (uintptr_t)file_parser->file_buffer + file_parser->next_chunk_offset;
    const uint32_t* current_file_ptr = (const uint32_t*)buffer_address;

    // Point the parsed chunk at the current location in the file.
    RgpFileChunkHeader* next_chunk = (RgpFileChunkHeader*)current_file_ptr;

    // It is possible to get stuck in loops by malformed data in the file since next_chunk_offset is updated
    // by next_chunk->size_in_bytes. Harden for that outside where we validate other chunk data.
    file_parser->next_chunk_offset += next_chunk->size_in_bytes;
    *parsed_chunk = next_chunk;

    return true;
}

/// @brief Validates an already-opened Ubertrace (RDF-based) RGP file.
///
/// Checks that all required RGP chunks are present and that the SPM session,
/// if present, contains at least one timestamp. Takes ownership of chunk_file
/// and closes it before returning.
///
/// @param [in] chunk_file An open RDF chunk file handle.
///
/// @return The validation status.
static RgpFileValidatorStatus RgpValidateUbertraceProfileData(rdfChunkFile* chunk_file)
{
    RgpFileValidatorStatus ret_val = kRgpFileValidatorStatusOk;

    // Verify all required chunks are present.
    static constexpr const char* kRequiredChunks[] = {
        kRgpRdfAsicInfoId,
        kRgpRdfApiInfoId,
        kRgpRdfTraceConfigId,
        kRgpRdfSqttDataId,
        kRgpRdfQueueInfoId,
        kRgpRdfQueueEventId,
        kRgpRdfClockCalibrationId,
    };

    for (const char* chunk_id : kRequiredChunks)
    {
        int64_t chunk_count = 0;
        rdfChunkFileGetChunkCount(chunk_file, chunk_id, &chunk_count);
        if (chunk_count == 0)
        {
            ret_val = kRgpFileValidatorStatusMissingChunkError;
            break;
        }
    }

    // If required chunks are all present, validate SPM data if a session chunk exists.
    if (ret_val == kRgpFileValidatorStatusOk)
    {
        int64_t spm_session_count = 0;
        rdfChunkFileGetChunkCount(chunk_file, kRgpRdfSpmSessionId, &spm_session_count);

        if (spm_session_count > 0)
        {
            int64_t spm_header_size = 0;
            if (rdfChunkFileGetChunkHeaderSize(chunk_file, kRgpRdfSpmSessionId, 0, &spm_header_size) != rdfResultOk ||
                spm_header_size != static_cast<int64_t>(sizeof(SpmSessionHeader)))
            {
                ret_val = kRgpFileValidatorStatusParserError;
            }
            else
            {
                SpmSessionHeader spm_header{};
                if (rdfChunkFileReadChunkHeader(chunk_file, kRgpRdfSpmSessionId, 0, &spm_header) != rdfResultOk)
                {
                    ret_val = kRgpFileValidatorStatusParserError;
                }
                else if (spm_header.num_timestamps == 0)
                {
                    ret_val = kRgpFileValidatorStatusSpmError;
                }
            }
        }
    }

    rdfChunkFileClose(&chunk_file);
    return ret_val;
}

RgpFileValidatorStatus RgpValidateProfileData(const QString& file_path)
{
    // Try Ubertrace (RDF-based) validation first — it is the default capture mode.
    // Use rdfStreamOpenFile + rdfChunkFileOpenStream instead of rdfChunkFileOpenFile
    // so that we can explicitly close the stream via rdfStreamClose.  The latter
    // properly calls fclose() on the underlying FILE*, whereas rdfChunkFileClose
    // alone leaks the file handle (IStream destructor is empty).
    const QByteArray path_utf8 = file_path.toUtf8();
    rdfStream*       stream    = nullptr;
    if (rdfStreamOpenFile(path_utf8.constData(), &stream) == rdfResultOk)
    {
        rdfChunkFile* chunk_file = nullptr;
        if (rdfChunkFileOpenStream(stream, &chunk_file) == rdfResultOk)
        {
            const auto result = RgpValidateUbertraceProfileData(chunk_file);
            rdfStreamClose(&stream);
            return result;
        }
        rdfStreamClose(&stream);
    }

    // Not an Ubertrace file — fall back to legacy RGP format validation.
    QFile sqtt_file_wrapper(file_path);
    if (!sqtt_file_wrapper.open(QIODevice::ReadOnly))
    {
        return kRgpFileValidatorStatusFileReadError;
    }

    const qint64 file_size = sqtt_file_wrapper.size();
    if (file_size <= 0 || file_size > static_cast<qint64>(INT32_MAX))
    {
        return kRgpFileValidatorStatusFileReadError;
    }

    const auto size_in_bytes = static_cast<size_t>(file_size);
    void*      file_buffer   = malloc(size_in_bytes);
    if (file_buffer == nullptr)
    {
        return kRgpFileValidatorStatusMemoryError;
    }

    const qint64 read_bytes = sqtt_file_wrapper.read(static_cast<char*>(file_buffer), file_size);
    sqtt_file_wrapper.close();

    if (read_bytes != file_size)
    {
        free(file_buffer);
        return kRgpFileValidatorStatusFileReadError;
    }

    RgpFileParser rgp_file_parser;
    if (!RgpFileParserCreateFromBuffer(&rgp_file_parser, file_buffer, size_in_bytes))
    {
        free(file_buffer);
        return kRgpFileValidatorStatusParserError;
    }

    RgpFileValidatorStatus ret_val            = kRgpFileValidatorStatusOk;
    RgpFileChunkHeader*    current_file_chunk = NULL;

    while (RgpFileParserParseNextChunk(&rgp_file_parser, &current_file_chunk) && (ret_val == kRgpFileValidatorStatusOk))
    {
        if ((current_file_chunk == nullptr) || (current_file_chunk->size_in_bytes <= 0) || (rgp_file_parser.next_chunk_offset < 0) ||
            (rgp_file_parser.next_chunk_offset > static_cast<int32_t>(size_in_bytes)) ||
            ((size_t(current_file_chunk->size_in_bytes) < sizeof(RgpFileChunkHeader)) ||
             (current_file_chunk->size_in_bytes > static_cast<int32_t>(size_in_bytes))))
        {
            ret_val = kRgpFileValidatorStatusParserError;
        }
        else if (current_file_chunk->chunk_identifier.chunk_type == kRgpFileChunkTypeSpmDatabase)
        {
            if (current_file_chunk->version_major >= kRgpFileChunkTypeSpmDbSizeMembersAddedMajorVersion)
            {
                const RgpFileChunkSpmDb* spm_db = reinterpret_cast<const RgpFileChunkSpmDb*>(current_file_chunk);
                ret_val                         = spm_db->number_of_timestamps == 0 ? kRgpFileValidatorStatusSpmError : kRgpFileValidatorStatusOk;
            }
            else
            {
                const RgpFileChunkSpmDbV1* spm_db_v1 = reinterpret_cast<const RgpFileChunkSpmDbV1*>(current_file_chunk);
                ret_val                              = spm_db_v1->number_of_timestamps == 0 ? kRgpFileValidatorStatusSpmError : kRgpFileValidatorStatusOk;
            }
        }
    }

    free(file_buffer);
    return ret_val;
}
