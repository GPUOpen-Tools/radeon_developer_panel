// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Functions to read SPM database, and calculate derived counter values.

#include "counters/rgp_spm_database.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>

#include <rgp_file_format.h>

#include <dev_trace_common.h>

#define RGP_ASSERT(x) assert(x)
#define RGP_ASSERT_FAIL(x) assert(x)
#define RGP_ASSERT_MESSAGE(x, y)     \
    if (!(x))                        \
    {                                \
        if (logger_ != nullptr)      \
        {                            \
            logger_->LogError(y, 0); \
        }                            \
                                     \
        assert(x);                   \
    }

#define RgpPrint(x, ...)                       \
    if (logger_ != nullptr)                    \
    {                                          \
        logger_->LogInfo(x, 0, ##__VA_ARGS__); \
    }

/// Helper macro to return error code y from a function when a specific condition, x, is not met.
#define RGP_RETURN_ON_ERROR(x, y) \
    if (!(x))                     \
    {                             \
        return (y);               \
    }

RgpSpmDataBase::RgpSpmDataBase()
    : derived_counters_calculated_(false)
    , counter_info_1_2_supported_(false)
    , counter_info_2_0_supported_(false)
    , gpa_func_table_(nullptr)
    , gpa_counter_context_(nullptr)
{
    memset(&raw_spm_db_, 0, sizeof(RawRgpSpmDataBase));
}

RgpSpmDataBase::~RgpSpmDataBase()
{
    free(raw_spm_db_.timestamps);
    raw_spm_db_.timestamps = nullptr;
    free(raw_spm_db_.spm_counter_info);
    raw_spm_db_.spm_counter_info = nullptr;
    free(raw_spm_db_.counter_data);
    raw_spm_db_.counter_data = nullptr;

    raw_spm_db_.number_of_timestamps       = 0;
    raw_spm_db_.number_of_spm_counter_info = 0;

    if (nullptr != gpa_counter_context_ && nullptr != gpa_func_table_)
    {
        GpaStatus status = gpa_func_table_->GpaCounterLibCloseCounterContext(gpa_counter_context_);
        RGP_ASSERT_MESSAGE(status == kGpaStatusOk, "GPA Error");
    }
}

// When the SPM chunk added the event index to SpmCounterInfo (v1.2)
static const int kRgpFileChunkTypeSpmDbAddedEventIndexMajorVersion = 1;  ///< Major version of the SPM Db chunk when the event index was added.
static const int kRgpFileChunkTypeSpmDbAddedEventIndexMinorVersion = 2;  ///< Minor version of the SPM Db chunk when the event index was added.

// When the SPM chunk added the sampling interval (v1.3)
static const int kRgpFileChunkTypeSpmDbAddedSamplingIntervalMajorVersion = 1;  ///< Major version of the Spm Db chunk when the sampling interval was added.
static const int kRgpFileChunkTypeSpmDbAddedSamplingIntervalMinorVersion = 3;  ///< Minor version of the Spm Db chunk when the sampling interval was added.

RgpErrorCode RgpSpmDataBase::Initialize(const void*                                           spm_db_chunk,
                                        RgpFileChunkAsicInfo                                  asic_info,
                                        std::vector<DerivedCounterInfo>                       requested_derived_counters,
                                        std::unordered_map<std::string, CounterComponentInfo> requested_counter_components,
                                        bool                                                  filter_bad_spm_data,
                                        const std::shared_ptr<devtrace::Logger>&              logger)
{
    logger_ = logger;

    if (nullptr == spm_db_chunk)
    {
        RGP_ASSERT(spm_db_chunk != nullptr);
        return kRgpErrorInvalidPointer;
    }
    // Load the info given by RDP

    asic_info_.gfx_ip_level                   = asic_info.gfx_ip_level;
    asic_info_.device_id                      = asic_info.device_id;
    asic_info_.device_revision_id             = asic_info.device_revision_id;
    asic_info_.shader_engines                 = asic_info.shader_engines;
    asic_info_.compute_unit_per_shader_engine = asic_info.compute_unit_per_shader_engine;
    asic_info_.simd_per_compute_unit          = asic_info.simd_per_compute_unit;

    requested_derived_counters_   = requested_derived_counters;
    requested_counter_components_ = requested_counter_components;
    filter_bad_spm_data_          = filter_bad_spm_data;

    FillExpectedCounters(requested_counter_components);

    // Now load the SPM DB
    const RgpFileChunkHeader* chunk_header = reinterpret_cast<const RgpFileChunkHeader*>(spm_db_chunk);
    if (chunk_header->version_major >= kRgpFileChunkTypeSpmDbSizeMembersAddedMajorVersion)
    {
        counter_info_2_0_supported_            = true;
        const RgpFileChunkSpmDb* spm_db_buffer = reinterpret_cast<const RgpFileChunkSpmDb*>(spm_db_chunk);
        raw_spm_db_.flags                      = spm_db_buffer->flags;
        raw_spm_db_.number_of_timestamps       = spm_db_buffer->number_of_timestamps;
        raw_spm_db_.number_of_spm_counter_info = spm_db_buffer->number_of_spm_counter_info;
        raw_spm_db_.sampling_interval          = spm_db_buffer->sampling_interval;

        // Timestamps.
        const size_t size_of_timestamp_data = raw_spm_db_.number_of_timestamps * sizeof(uint64_t);
        raw_spm_db_.timestamps              = static_cast<uint64_t*>(malloc(size_of_timestamp_data));
        RGP_ASSERT_MESSAGE(raw_spm_db_.timestamps, "Couldn't allocate memory for SPM structures.");
        RGP_RETURN_ON_ERROR(raw_spm_db_.timestamps, kRgpErrorOutOfMemory);
        memset(raw_spm_db_.timestamps, 0, size_of_timestamp_data);

        const unsigned char* src = reinterpret_cast<const unsigned char*>(spm_db_buffer) + spm_db_buffer->preamble_size;
        memcpy(raw_spm_db_.timestamps, src, size_of_timestamp_data);

        MapTimestamps();

        // CounterInfo.
        const size_t counter_info_buffer_size = raw_spm_db_.number_of_spm_counter_info * spm_db_buffer->spm_counter_info_size;
        raw_spm_db_.spm_counter_info          = malloc(counter_info_buffer_size);
        RGP_ASSERT_MESSAGE(raw_spm_db_.spm_counter_info, "Couldn't allocate memory for SPM structures.");
        RGP_RETURN_ON_ERROR(raw_spm_db_.spm_counter_info, kRgpErrorOutOfMemory);
        memset(raw_spm_db_.spm_counter_info, 0, counter_info_buffer_size);

        src += size_of_timestamp_data;
        memcpy(raw_spm_db_.spm_counter_info, src, counter_info_buffer_size);

        // CounterData.
        // First, calculate the size of each timestamp's data (based on each counter's reported size).
        size_t size_of_one_timestamp = 0;
        for (uint32_t i = 0; i < raw_spm_db_.number_of_spm_counter_info; i++)
        {
            size_of_one_timestamp += reinterpret_cast<SpmCounterInfo_V2*>(raw_spm_db_.spm_counter_info)[i].data_size;
        }

        // Next, calculate the total memory needed for all data and allocate that memory.
        const size_t size_of_counter_data = static_cast<size_t>(raw_spm_db_.number_of_timestamps) * size_of_one_timestamp;
        RGP_ASSERT_MESSAGE(size_of_counter_data > 0, "No counter data -- nothing to allocate.");
        RGP_RETURN_ON_ERROR(size_of_counter_data > 0, kRgpErrorMalformedData);
        raw_spm_db_.counter_data = reinterpret_cast<uint16_t*>(malloc(size_of_counter_data));
        RGP_ASSERT_MESSAGE(raw_spm_db_.counter_data, "Couldn't allocate memory for SPM structures.");
        RGP_RETURN_ON_ERROR(raw_spm_db_.counter_data, kRgpErrorOutOfMemory);
        memset(raw_spm_db_.counter_data, 0, size_of_counter_data);

        // Then, copy the raw data into the allocated memory.
        src += counter_info_buffer_size;
        memcpy(raw_spm_db_.counter_data, src, size_of_counter_data);

        // Patch counter offsets: subtract (timestamp bytes + spm_counter_info bytes)
        // which is the start of the first offset.
        const size_t offset = size_of_timestamp_data + counter_info_buffer_size;

        RGP_ASSERT(offset == reinterpret_cast<SpmCounterInfo_V2*>(raw_spm_db_.spm_counter_info)[0].data_offset);
        for (uint32_t i = 0; i < raw_spm_db_.number_of_spm_counter_info; ++i)
        {
            reinterpret_cast<SpmCounterInfo_V2*>(raw_spm_db_.spm_counter_info)[i].data_offset -= (uint32_t)offset;
        }
    }
    else
    {
        const RgpFileChunkSpmDbV1* spm_db_buffer = reinterpret_cast<const RgpFileChunkSpmDbV1*>(spm_db_chunk);
        raw_spm_db_.flags                        = spm_db_buffer->flags;
        raw_spm_db_.number_of_timestamps         = spm_db_buffer->number_of_timestamps;
        raw_spm_db_.number_of_spm_counter_info   = spm_db_buffer->number_of_spm_counter_info;
        raw_spm_db_.sampling_interval            = spm_db_buffer->sampling_interval;
        bool sampling_interval_1_3_supported     = spm_db_buffer->header.version_major > kRgpFileChunkTypeSpmDbAddedSamplingIntervalMajorVersion ||
                                               (spm_db_buffer->header.version_major == kRgpFileChunkTypeSpmDbAddedSamplingIntervalMajorVersion &&
                                                spm_db_buffer->header.version_minor >= kRgpFileChunkTypeSpmDbAddedSamplingIntervalMinorVersion);

        // Prior to v1.3, there is no sampling interval in the chunk -- so we hardcode to 4096, the value used by default.
        raw_spm_db_.sampling_interval = sampling_interval_1_3_supported ? spm_db_buffer->sampling_interval : 4096;

        // Timestamps.
        raw_spm_db_.timestamps = static_cast<uint64_t*>(malloc(raw_spm_db_.number_of_timestamps * sizeof(uint64_t)));
        RGP_ASSERT_MESSAGE(raw_spm_db_.timestamps, "Couldn't allocate memory for SPM structures.");
        RGP_RETURN_ON_ERROR(raw_spm_db_.timestamps, kRgpErrorOutOfMemory);
        memset(raw_spm_db_.timestamps, 0, raw_spm_db_.number_of_timestamps * sizeof(uint64_t));

        const unsigned char* src = reinterpret_cast<const unsigned char*>(spm_db_buffer) + sizeof(RgpFileChunkSpmDbV1);
        if (!sampling_interval_1_3_supported)
        {
            // If the chunk is not at version 1.3, subtract the size of the sampling interval, since that member wasn't written by PAL prior to v1.3.
            src -= sizeof(spm_db_buffer->sampling_interval);
        }

        memcpy(raw_spm_db_.timestamps, src, raw_spm_db_.number_of_timestamps * sizeof(uint64_t));
        mapped_timestamps_.resize(raw_spm_db_.number_of_timestamps);

        for (uint32_t i = 0; i < raw_spm_db_.number_of_timestamps; i++)
        {
            mapped_timestamps_[i] = {raw_spm_db_.timestamps[i], true};
        }

        counter_info_1_2_supported_ = spm_db_buffer->header.version_major > kRgpFileChunkTypeSpmDbAddedEventIndexMajorVersion ||
                                      (spm_db_buffer->header.version_major == kRgpFileChunkTypeSpmDbAddedEventIndexMajorVersion &&
                                       spm_db_buffer->header.version_minor >= kRgpFileChunkTypeSpmDbAddedEventIndexMinorVersion);

        size_t counter_info_size = counter_info_1_2_supported_ ? sizeof(SpmCounterInfo_V1_2) : sizeof(SpmCounterInfo);

        // CounterInfo.
        raw_spm_db_.spm_counter_info = malloc(raw_spm_db_.number_of_spm_counter_info * counter_info_size);
        RGP_ASSERT_MESSAGE(raw_spm_db_.spm_counter_info, "Couldn't allocate memory for SPM structures.");
        RGP_RETURN_ON_ERROR(raw_spm_db_.spm_counter_info, kRgpErrorOutOfMemory);
        memset(raw_spm_db_.spm_counter_info, 0, raw_spm_db_.number_of_spm_counter_info * counter_info_size);

        src += raw_spm_db_.number_of_timestamps * sizeof(uint64_t);
        memcpy(raw_spm_db_.spm_counter_info, src, raw_spm_db_.number_of_spm_counter_info * counter_info_size);

        // CounterData.
        const size_t size_of_counter_data =
            static_cast<size_t>(raw_spm_db_.number_of_timestamps) * static_cast<size_t>(raw_spm_db_.number_of_spm_counter_info) * sizeof(uint16_t);
        raw_spm_db_.counter_data = reinterpret_cast<uint16_t*>(malloc(size_of_counter_data));
        RGP_ASSERT_MESSAGE(raw_spm_db_.counter_data, "Couldn't allocate memory for SPM structures.");
        RGP_RETURN_ON_ERROR(raw_spm_db_.counter_data, kRgpErrorOutOfMemory);
        memset(raw_spm_db_.counter_data, 0, size_of_counter_data);

        src += raw_spm_db_.number_of_spm_counter_info * counter_info_size;
        memcpy(raw_spm_db_.counter_data, src, size_of_counter_data);

        // Patch counter offsets: subtract (timestamp bytes + spm_counter_info bytes)
        // which is the start of the first offset.
        const size_t offset = (raw_spm_db_.number_of_timestamps * sizeof(uint64_t)) + (raw_spm_db_.number_of_spm_counter_info * counter_info_size);

        if (counter_info_1_2_supported_)
        {
            RGP_ASSERT(offset == reinterpret_cast<SpmCounterInfo_V1_2*>(raw_spm_db_.spm_counter_info)[0].data_offset);
            for (uint32_t i = 0; i < raw_spm_db_.number_of_spm_counter_info; ++i)
            {
                reinterpret_cast<SpmCounterInfo_V1_2*>(raw_spm_db_.spm_counter_info)[i].data_offset -= (uint32_t)offset;
            }
        }
        else
        {
            RGP_ASSERT(offset == reinterpret_cast<SpmCounterInfo*>(raw_spm_db_.spm_counter_info)[0].data_offset);
            for (uint32_t i = 0; i < raw_spm_db_.number_of_spm_counter_info; ++i)
            {
                reinterpret_cast<SpmCounterInfo*>(raw_spm_db_.spm_counter_info)[i].data_offset -= (uint32_t)offset;
            }
        }
    }
    return kRgpOk;
}

static constexpr const char* kSpmSessionChunkId = "SpmSession";
static constexpr const char* kSpmDataChunkId    = "SpmCounterData";

RgpErrorCode RgpSpmDataBase::InitializeRdf(rdfChunkFile*                                         chunk_file,
                                           RgpFileChunkAsicInfoRdfV3                             asic_info,
                                           std::vector<DerivedCounterInfo>                       requested_derived_counters,
                                           std::unordered_map<std::string, CounterComponentInfo> requested_counter_components,
                                           bool                                                  filter_bad_spm_data,
                                           const std::shared_ptr<devtrace::Logger>&              logger)
{
    logger_ = logger;

    asic_info_.gfx_ip_level                   = asic_info.v2.gfx_ip_level.major + 1;
    asic_info_.device_id                      = asic_info.v2.device_id;
    asic_info_.device_revision_id             = asic_info.v2.device_revision_id;
    asic_info_.shader_engines                 = asic_info.v2.shader_engines;
    asic_info_.compute_unit_per_shader_engine = asic_info.v2.compute_unit_per_shader_engine;
    asic_info_.simd_per_compute_unit          = asic_info.v2.simd_per_compute_unit;

    requested_derived_counters_   = requested_derived_counters;
    requested_counter_components_ = requested_counter_components;
    filter_bad_spm_data_          = filter_bad_spm_data;

    FillExpectedCounters(requested_counter_components);

    // Read in the session header
    int64_t session_count;
    if (rdfChunkFileGetChunkCount(chunk_file, kSpmSessionChunkId, &session_count) != rdfResultOk || session_count == 0)
    {
        return kRgpErrorMalformedData;
    }

    int64_t header_size{};
    if (rdfChunkFileGetChunkHeaderSize(chunk_file, kSpmSessionChunkId, 0, &header_size) != rdfResultOk || header_size != sizeof(SpmSessionHeader))
    {
        return kRgpErrorMalformedData;
    }

    SpmSessionHeader header;
    if (rdfChunkFileReadChunkHeader(chunk_file, kSpmSessionChunkId, 0, &header) != rdfResultOk)
    {
        return kRgpErrorMalformedData;
    }

    raw_spm_db_.flags                      = header.flags;
    raw_spm_db_.number_of_timestamps       = header.num_timestamps;
    raw_spm_db_.number_of_spm_counter_info = header.num_spm_counters;
    raw_spm_db_.sampling_interval          = header.sampling_interval;
    counter_info_3_0_supported_            = true;

    const size_t timestamp_data_size = raw_spm_db_.number_of_timestamps * sizeof(uint64_t);
    raw_spm_db_.timestamps           = static_cast<uint64_t*>(malloc(timestamp_data_size));

    RGP_ASSERT_MESSAGE(raw_spm_db_.timestamps, "Couldn't allocate memory for SPM structures.");
    RGP_RETURN_ON_ERROR(raw_spm_db_.timestamps, kRgpErrorOutOfMemory);

    // Read in the timestamps from the session chunk
    int64_t chunk_size;
    if (rdfChunkFileGetChunkDataSize(chunk_file, kSpmSessionChunkId, 0, &chunk_size) != rdfResultOk || static_cast<size_t>(chunk_size) != timestamp_data_size)
    {
        return kRgpErrorMalformedData;
    }

    if (rdfChunkFileReadChunkData(chunk_file, kSpmSessionChunkId, 0, raw_spm_db_.timestamps) != rdfResultOk)
    {
        return kRgpErrorMalformedData;
    }

    MapTimestamps();

    // Read in the headers of the SPM data
    int64_t num_data_chunks;
    if (rdfChunkFileGetChunkCount(chunk_file, kSpmDataChunkId, &num_data_chunks) != rdfResultOk || num_data_chunks != header.num_spm_counters)
    {
        return kRgpErrorMalformedData;
    }

    const size_t counter_info_buffer_size = raw_spm_db_.number_of_spm_counter_info * sizeof(SpmCounterDataHeader);
    raw_spm_db_.spm_counter_info          = malloc(counter_info_buffer_size);

    RGP_ASSERT_MESSAGE(raw_spm_db_.spm_counter_info, "Couldn't allocate memory for SPM structures.");
    RGP_RETURN_ON_ERROR(raw_spm_db_.spm_counter_info, kRgpErrorOutOfMemory);

    size_t size_of_one_timestamp = 0;
    for (int chunk_num = 0; chunk_num < static_cast<int>(num_data_chunks); ++chunk_num)
    {
        if (rdfChunkFileGetChunkHeaderSize(chunk_file, kSpmDataChunkId, chunk_num, &header_size) != rdfResultOk || header_size != sizeof(SpmCounterDataHeader))
        {
            return kRgpErrorMalformedData;
        }

        SpmCounterDataHeader* data_header = reinterpret_cast<SpmCounterDataHeader*>(raw_spm_db_.spm_counter_info) + chunk_num;
        if (rdfChunkFileReadChunkHeader(chunk_file, kSpmDataChunkId, chunk_num, data_header) != rdfResultOk || data_header->data_size == 0)
        {
            return kRgpErrorMalformedData;
        }

        size_of_one_timestamp += data_header->data_size;
    }

    // Read the raw counter data
    const size_t size_of_counter_data = static_cast<size_t>(raw_spm_db_.number_of_timestamps) * size_of_one_timestamp;
    RGP_ASSERT_MESSAGE(size_of_counter_data > 0, "No counter data -- nothing to allocate.");
    RGP_RETURN_ON_ERROR(size_of_counter_data > 0, kRgpErrorMalformedData);

    raw_spm_db_.counter_data = reinterpret_cast<uint16_t*>(malloc(size_of_counter_data));
    RGP_ASSERT_MESSAGE(raw_spm_db_.counter_data, "Couldn't allocate memory for SPM structures.");
    RGP_RETURN_ON_ERROR(raw_spm_db_.counter_data, kRgpErrorOutOfMemory);

    int64_t                offset = 0;
    std::vector<GpaUInt32> counter_offsets;

    for (int chunk_num = 0; chunk_num < static_cast<int>(num_data_chunks); ++chunk_num)
    {
        if (rdfChunkFileGetChunkDataSize(chunk_file, kSpmDataChunkId, chunk_num, &chunk_size) != rdfResultOk)
        {
            return kRgpErrorMalformedData;
        }

        if (static_cast<size_t>(chunk_size + offset) > size_of_counter_data)
        {
            return kRgpErrorMalformedData;
        }

        uint8_t* write_location = reinterpret_cast<uint8_t*>(raw_spm_db_.counter_data) + offset;
        if (rdfChunkFileReadChunkData(chunk_file, kSpmDataChunkId, chunk_num, write_location) != rdfResultOk)
        {
            return kRgpErrorMalformedData;
        }

        counter_offsets.push_back(static_cast<GpaUInt32>(offset / sizeof(uint16_t)));
        offset += chunk_size;
    }

    raw_spm_db_.counter_offsets = std::move(counter_offsets);
    return kRgpOk;
}

void RgpSpmDataBase::FillExpectedCounters(const std::unordered_map<std::string, CounterComponentInfo>& requested_counter_components)
{
    // First, add the actual counters that will be shown on the graph.
    for (size_t i = 0; i < requested_derived_counters_.size(); i++)
    {
        expected_derived_counters_.push_back({requested_derived_counters_[i], false, false});
    }

    // Now add counter components needed for components.
    for (const auto& pair : requested_counter_components)
    {
        const CounterComponentInfo& counter_component_info = pair.second;
        for (size_t i = 0; i < counter_component_info.derived_counter_list.size(); i++)
        {
            expected_derived_counters_.push_back({DerivedCounterInfo({counter_component_info.derived_counter_list[i], "", kCounterUnknown}), true, false});
        }
    }
}

void RgpSpmDataBase::MapTimestamps()
{
    mapped_timestamps_.resize(raw_spm_db_.number_of_timestamps);
    for (uint32_t i = 0; i < raw_spm_db_.number_of_timestamps; i++)
    {
        mapped_timestamps_[i] = {raw_spm_db_.timestamps[i], true};
    }
}

uint32_t RgpSpmDataBase::GetSamplingInterval() const
{
    return raw_spm_db_.sampling_interval;
}

size_t RgpSpmDataBase::GetNumberOfTimestamps() const
{
    size_t number_of_timestamps = 0;

    if (asic_info_.gfx_ip_level >= kRgpGfxLevelMinGfx10)
    {
        number_of_timestamps = mapped_timestamps_.size();
    }

    return number_of_timestamps;
}

RgpErrorCode RgpSpmDataBase::GetTimestamp(size_t timestamp_index, uint64_t& timestamp) const
{
    RgpErrorCode error_code = kRgpOk;

    if (timestamp_index < mapped_timestamps_.size())
    {
        timestamp = mapped_timestamps_[timestamp_index].timestamp;
    }
    else
    {
        error_code = kRgpErrorIndexOutOfRange;
    }

    return error_code;
}

size_t RgpSpmDataBase::GetNumberOfCounters(bool derived, const ProgressCallback& progress_callback, const std::atomic_bool* should_abort)
{
    size_t number_of_counters = 0;

    if (asic_info_.gfx_ip_level >= kRgpGfxLevelMinGfx10 && raw_spm_db_.number_of_spm_counter_info > 0)
    {
        if (derived)
        {
            const RgpErrorCode error_code = CalculateDerivedCounters(progress_callback, should_abort);
            if (kRgpOk == error_code)
            {
                number_of_counters = available_derived_counters_.size();
            }
            else if (error_code == kRgpErrorAborted)
            {
                return static_cast<size_t>(-1);
            }
        }
        else
        {
            number_of_counters = raw_spm_db_.number_of_spm_counter_info;
        }
    }

    return number_of_counters;
}

size_t RgpSpmDataBase::GetAdjustedIndex(size_t counter_index)
{
    size_t result = counter_index;

    // We need to map the counter_index passed in to the index that should be used with
    // the counter_values_ list. When filtering out counters with bad data, we remove them
    // from the list of availble counters, but leave their data in the counter_values list.
    RGP_RETURN_ON_ERROR(counter_index < counter_value_is_bad_.size(), counter_index);
    size_t good_counter_count = 0;
    for (size_t i = 0; i <= counter_value_is_bad_.size(); i++)
    {
        if (counter_value_is_bad_[i] == 1)
        {
            result++;
        }
        else
        {
            if (good_counter_count == counter_index)
            {
                break;
            }
            good_counter_count++;
        }
    }

    return result;
}

RgpErrorCode RgpSpmDataBase::GetCounterName(size_t counter_index, std::string& counter_name, bool derived)
{
    return GetCounterInfo(counter_index, kCounterQueryTypeName, derived, counter_name);
}

RgpErrorCode RgpSpmDataBase::GetGpaCounterName(size_t counter_index, std::string& counter_name, bool derived)
{
    return GetCounterInfo(counter_index, kCounterQueryTypeGpaName, derived, counter_name);
}

RgpErrorCode RgpSpmDataBase::GetCounterDescription(size_t counter_index, std::string& counter_desc, bool derived)
{
    return GetCounterInfo(counter_index, kCounterQueryTypeDescription, derived, counter_desc);
}

RgpErrorCode RgpSpmDataBase::GetCounterIdentifier(size_t counter_index, CounterIdentifier& counter_id, bool derived)
{
    RgpErrorCode error_code = kRgpOk;

    if (derived)
    {
        error_code = CalculateDerivedCounters();
        if (kRgpOk == error_code)
        {
            RGP_ASSERT_MESSAGE(counter_index < available_derived_counters_.size(), "counter_index out of range");
            if (counter_index < available_derived_counters_.size())
            {
                counter_id = available_derived_counters_[counter_index].counter_info.counter_id;
            }
            else
            {
                error_code = kRgpErrorIndexOutOfRange;
            }
        }
    }
    else
    {
        RGP_ASSERT_MESSAGE(counter_index < raw_spm_db_.number_of_spm_counter_info, "counter_index out of range");
        if (counter_index < raw_spm_db_.number_of_spm_counter_info)
        {
            // GPA hardware counters are always *unknown*.
            counter_id = kCounterUnknown;
        }
        else
        {
            error_code = kRgpErrorIndexOutOfRange;
        }
    }

    return error_code;
}

RgpErrorCode RgpSpmDataBase::GetCounterUsageType(size_t counter_index, CounterUsageType& counter_usage_type, bool derived)
{
    RgpErrorCode error_code = kRgpOk;

    if (derived)
    {
        error_code = CalculateDerivedCounters();
        if (kRgpOk == error_code)
        {
            RGP_ASSERT_MESSAGE(counter_index < available_derived_counters_.size(), "counter_index out of range");
            if (counter_index < available_derived_counters_.size())
            {
                counter_usage_type = available_derived_counters_[counter_index].counter_usage_type;
            }
            else
            {
                error_code = kRgpErrorIndexOutOfRange;
            }
        }
    }
    else
    {
        RGP_ASSERT_MESSAGE(counter_index < raw_spm_db_.number_of_spm_counter_info, "counter_index out of range");
        if (counter_index < raw_spm_db_.number_of_spm_counter_info)
        {
            // GPA hardware counters are always *items*.
            counter_usage_type = kCounterUsageTypeItems;
        }
        else
        {
            error_code = kRgpErrorIndexOutOfRange;
        }
    }

    return error_code;
}

RgpErrorCode RgpSpmDataBase::GetCounterValue(size_t timestamp_index, size_t counter_index, double& counter_value, bool derived)
{
    RgpErrorCode error_code = kRgpOk;

    if (derived)
    {
        error_code = CalculateDerivedCounters();
        if (kRgpOk == error_code)
        {
            RGP_ASSERT_MESSAGE(counter_index < available_derived_counters_.size(), "counter_index out of range");
            if (counter_index < available_derived_counters_.size() && timestamp_index < raw_spm_db_.number_of_timestamps)
            {
                if (mapped_timestamps_[timestamp_index].timestamp_is_valid)
                {
                    counter_value = counter_values_[GetAdjustedIndex(counter_index)][timestamp_index];
                }
                else
                {
#ifdef _DEBUG
                    // The following can be useful in debugging cases where counters are filtered out because a timestamp has been deemed invalid.
                    // Commented out now to avoid the perf penalty of potentially large number of RgpPrint calls. Uncomment when needed.
                    //if (counter_values_[counter_index][timestamp_index] > 0)
                    //{
                    //    RgpPrint("Counter {} at timestamp {} with value {} is ignored",
                    //             counter_index,
                    //             timestamp_index,
                    //             counter_values_[counter_index][timestamp_index]);
                    //}
#endif
                    counter_value = 0;
                }
            }
            else
            {
                error_code = kRgpErrorIndexOutOfRange;
            }
        }
    }
    else
    {
        RGP_ASSERT_MESSAGE(timestamp_index < raw_spm_db_.number_of_timestamps, "timestamp_index out of range");
        RGP_ASSERT_MESSAGE(counter_index < raw_spm_db_.number_of_spm_counter_info, "counter_index out of range");

        if (timestamp_index < raw_spm_db_.number_of_timestamps && counter_index < raw_spm_db_.number_of_spm_counter_info)
        {
            if (counter_info_1_2_supported_ || counter_info_2_0_supported_ || counter_info_3_0_supported_)
            {
                if (mapped_timestamps_[timestamp_index].timestamp_is_valid)
                {
                    size_t   offset_in_words    = 0;  // Index into raw_spm_db_.counter_data of the start of the samples for this counter.
                    uint32_t data_size_in_words = 1;  // Number of 16-bit words that make up the samples for the counter.
                    if (counter_info_3_0_supported_)
                    {
                        offset_in_words                          = raw_spm_db_.counter_offsets[counter_index];
                        const SpmCounterDataHeader& counter_info = reinterpret_cast<SpmCounterDataHeader*>(raw_spm_db_.spm_counter_info)[counter_index];
                        RGP_ASSERT(counter_info.data_size % 2 == 0);
                        data_size_in_words = counter_info.data_size / 2;
                    }
                    else if (counter_info_2_0_supported_)
                    {
                        const SpmCounterInfo_V2& counter_info = reinterpret_cast<SpmCounterInfo_V2*>(raw_spm_db_.spm_counter_info)[counter_index];
                        offset_in_words                       = counter_info.data_offset / 2;
                        RGP_ASSERT(counter_info.data_size % 2 == 0);
                        data_size_in_words = counter_info.data_size / 2;
                    }
                    else if (counter_info_1_2_supported_)
                    {
                        offset_in_words = reinterpret_cast<SpmCounterInfo_V1_2*>(raw_spm_db_.spm_counter_info)[counter_index].data_offset;
                    }

                    // Get the offset of the requested timestamp from the start of the counter's samples.
                    offset_in_words += (timestamp_index * data_size_in_words);
                    uint32_t this_value = raw_spm_db_.counter_data[offset_in_words];

                    // Now build up the actual counter value for cases where the counter is 32-bits (or larger).
                    while (data_size_in_words > 1)
                    {
                        data_size_in_words--;
                        uint16_t next_value = raw_spm_db_.counter_data[offset_in_words + data_size_in_words];
                        this_value += (next_value << (data_size_in_words * 16));
                    }
                    counter_value = this_value;
                }
                else
                {
                    counter_value = 0;
                }
            }
            else
            {
                error_code = kRgpErrorUnsupportedLowSpecVersion;
            }
        }
        else
        {
            error_code = kRgpErrorIndexOutOfRange;
        }
    }
    return error_code;
}

size_t RgpSpmDataBase::GetNumberOfCounterComponents(size_t counter_index, bool derived)
{
    size_t number_of_components = 0;

    /// Only derived counters have components.
    if (derived)
    {
        RgpErrorCode error_code = CalculateDerivedCounters();

        if (kRgpOk == error_code)
        {
            RGP_ASSERT_MESSAGE(counter_index < available_derived_counters_.size(), "counter_index out of range");

            if (counter_index < available_derived_counters_.size())
            {
                auto counter = requested_counter_components_.find(available_derived_counters_[counter_index].counter_info.gpa_counter_name);
                if (counter != requested_counter_components_.end())
                {
                    number_of_components = counter->second.component_list.size();
                }
            }
        }
    }

    return number_of_components;
}

RgpErrorCode RgpSpmDataBase::GetCounterComponentName(size_t counter_index, size_t component_index, std::string& component_name, bool derived)
{
    RgpErrorCode error_code = kRgpOk;

    /// Only derived counters have components.
    if (derived)
    {
        error_code = CalculateDerivedCounters();
        if (kRgpOk == error_code)
        {
            error_code = kRgpErrorIndexOutOfRange;
            RGP_ASSERT_MESSAGE(counter_index < available_derived_counters_.size(), "counter_index out of range");
            if (counter_index < available_derived_counters_.size())
            {
                auto counter = requested_counter_components_.find(available_derived_counters_[counter_index].counter_info.gpa_counter_name);
                if (counter != requested_counter_components_.end())
                {
                    RGP_ASSERT_MESSAGE(component_index < counter->second.component_list.size(), "component_index out of range");
                    if (component_index < counter->second.component_list.size())
                    {
                        component_name = counter->second.component_list[component_index].component_name;
                        error_code     = kRgpOk;
                    }
                }
            }
        }
    }
    else
    {
        // Non-derived counters do not have components -- return empty string.
        component_name.clear();
    }
    return error_code;
}

RgpErrorCode RgpSpmDataBase::GetCounterComponentDescription(size_t counter_index, size_t component_index, std::string& component_description, bool derived)
{
    RgpErrorCode error_code = kRgpOk;

    /// Only derived counters have components.
    if (derived)
    {
        error_code = CalculateDerivedCounters();
        if (kRgpOk == error_code)
        {
            error_code = kRgpErrorIndexOutOfRange;
            RGP_ASSERT_MESSAGE(counter_index < available_derived_counters_.size(), "counter_index out of range");
            if (counter_index < available_derived_counters_.size())
            {
                auto counter = requested_counter_components_.find(available_derived_counters_[counter_index].counter_info.gpa_counter_name);
                if (counter != requested_counter_components_.end())
                {
                    RGP_ASSERT_MESSAGE(component_index < counter->second.component_list.size(), "component_index out of range");
                    if (component_index < counter->second.component_list.size())
                    {
                        std::string counter_name = available_derived_counters_[counter_index].counter_info.gpa_counter_name;
                        auto        counter      = requested_counter_components_.find(counter_name);
                        std::string gpa_name     = counter->second.derived_counter_list[component_index];
                        for (auto& counter : available_counter_components_)
                        {
                            if (counter.counter_info.gpa_counter_name == gpa_name)
                            {
                                component_description = counter.counter_desc;
                                error_code            = kRgpOk;
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        // Non-derived counters do not have components -- return empty string.
        component_description.clear();
    }
    return error_code;
}

RgpErrorCode RgpSpmDataBase::GetCounterComponentUsageType(size_t counter_index, size_t component_index, CounterUsageType& usage_type, bool derived)
{
    RgpErrorCode error_code = kRgpOk;

    /// Only derived counters have components.
    if (derived)
    {
        error_code = CalculateDerivedCounters();
        if (kRgpOk == error_code)
        {
            error_code = kRgpErrorIndexOutOfRange;
            RGP_ASSERT_MESSAGE(counter_index < available_derived_counters_.size(), "counter_index out of range");
            if (counter_index < available_derived_counters_.size())
            {
                auto counter = requested_counter_components_.find(available_derived_counters_[counter_index].counter_info.gpa_counter_name);
                if (counter != requested_counter_components_.end())
                {
                    RGP_ASSERT_MESSAGE(component_index < counter->second.component_list.size(), "component_index out of range");
                    if (component_index < counter->second.component_list.size())
                    {
                        std::string counter_name = available_derived_counters_[counter_index].counter_info.gpa_counter_name;
                        auto        counter      = requested_counter_components_.find(counter_name);
                        std::string gpa_name     = counter->second.derived_counter_list[component_index];
                        for (auto& counter : available_counter_components_)
                        {
                            if (counter.counter_info.gpa_counter_name == gpa_name)
                            {
                                usage_type = counter.counter_usage_type;
                                error_code = kRgpOk;
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        // Non-derived counters have usage type items
        usage_type = kCounterUsageTypeItems;
    }
    return error_code;
}

double RgpSpmDataBase::CalculateComponentValue(const std::string& formula, CounterValueList& counter_values)
{
    // This is a pared-down version of the expression evaluator in GPA.
    // The following are supported in counter formulas:
    //   1) An integer: represents an index into the counter_values list
    //   2) "+": adds the two previous operands
    //
    // Example formulas:
    //  1) "0,1,+" :  counter_values[0] + counter_values[1]
    //  2) "0" : counter_values[0]
    //  3) "1" : counter_values[1]

    double component_value    = 0;
    bool   counter_formula_ok = true;

    if (!formula.empty())
    {
        CounterValueList  tokens;
        std::stringstream ss(formula);

        for (std::string cur_token; std::getline(ss, cur_token, ',') && counter_formula_ok;)
        {
            if (cur_token == "+")
            {
                if (tokens.size() > 1)
                {
                    double arg1 = tokens.back();
                    tokens.pop_back();
                    double arg2 = tokens.back();
                    tokens.pop_back();
                    tokens.push_back(arg1 + arg2);
                }
                else
                {
                    counter_formula_ok = false;
                }
            }
            else
            {
                try
                {
                    tokens.push_back(counter_values[static_cast<size_t>(std::stoull(cur_token))]);
                }
                catch ([[maybe_unused]] std::exception& e)
                {
                    counter_formula_ok = false;
                }
            }
        }

        if (counter_formula_ok)
        {
            RGP_ASSERT(tokens.size() == 1);
            if (tokens.size() == 1)
            {
                component_value = tokens.back();
            }
        }
        else
        {
            RGP_ASSERT_FAIL("bad counter component formula");
        }
    }

    return component_value;
}

/// @brief A helper structure used to adjust counter components when the heuristic identifies a counter whose component values are mismatched by a small amount.
///
/// This is only applied to cache counters whose values are calculated using "Requests" and "Misses", and the "Hits" component is calculated based on those values.
/// There are some rare occasions where Misses is slightly higher than Requests -- these have been determined to be "noise", so when we detect this, we set the number
/// of Misses to equal the number of Requests and we set the number of Hits to zero.
struct CounterComponentHeuristicHelper
{
    size_t timestamp_index;  ///< Index of the timestamp of the previous slightly incorrect counter component.
    size_t counter_index;    ///< Index of the counter of the previous slightly in correct counter component.
    size_t component_index;  ///< Index of the component whose value is used to override another component (Requests overrides Misses, in this case).
    bool   use_heuristic;    ///< True when the heuristic is used for the next component calculated for this timestamp and counter index.
};

static CounterComponentHeuristicHelper counter_component_heuristic_helper = {0, 0, 0, false};

RgpErrorCode RgpSpmDataBase::GetCounterComponentValue(size_t  timestamp_index,
                                                      size_t  counter_index,
                                                      size_t  component_index,
                                                      double& component_value,
                                                      bool    derived)
{
    RgpErrorCode error_code = kRgpOk;

    /// Only derived counters have components.
    if (derived)
    {
        error_code = CalculateDerivedCounters();
        if (kRgpOk == error_code)
        {
            error_code = kRgpErrorIndexOutOfRange;
            RGP_ASSERT_MESSAGE(counter_index < available_derived_counters_.size(), "counter_index out of range");
            if (counter_index < available_derived_counters_.size() && timestamp_index < raw_spm_db_.number_of_timestamps)
            {
                std::string counter_name = available_derived_counters_[counter_index].counter_info.gpa_counter_name;
                auto        counter      = requested_counter_components_.find(counter_name);
                if (counter != requested_counter_components_.end())
                {
                    RGP_ASSERT_MESSAGE(component_index < counter->second.component_list.size(), "component_index out of range");
                    if (component_index < counter->second.component_list.size())
                    {
                        if (mapped_timestamps_[timestamp_index].timestamp_is_valid)
                        {
                            CounterValueList counter_vals;  // List of counter values for the extra derived counters used for components.
                            for (auto extra_counter = counter->second.derived_counter_list.begin(); extra_counter != counter->second.derived_counter_list.end();
                                 ++extra_counter)
                            {
                                uint32_t skipped_expected_counter_count = 0;
                                for (size_t i = 0; i < expected_derived_counters_.size(); i++)
                                {
                                    const auto& derived_counter = expected_derived_counters_[i];
                                    if (!derived_counter.is_available)
                                    {
                                        skipped_expected_counter_count++;
                                    }

                                    if (derived_counter.is_component_counter && derived_counter.is_available &&
                                        (derived_counter.counter_info.gpa_counter_name == *extra_counter))
                                    {
                                        counter_vals.push_back(counter_values_[i - skipped_expected_counter_count][timestamp_index]);
                                        break;
                                    }
                                }
                            }
                            if (counter_component_heuristic_helper.use_heuristic && counter_component_heuristic_helper.counter_index == counter_index &&
                                counter_component_heuristic_helper.timestamp_index)
                            {
                                // This is a component that needs to use the value from a previous component.
                                component_value = CalculateComponentValue(
                                    counter->second.component_list[counter_component_heuristic_helper.component_index].component_formula, counter_vals);
                                counter_component_heuristic_helper.use_heuristic = false;
                            }
                            else
                            {
                                component_value = CalculateComponentValue(counter->second.component_list[component_index].component_formula, counter_vals);
                            }
                            if (component_value < 0)
                            {
                                // The calculated component value was less than zero. If this is one of the counters where we use a heuristic to account for a slightly wrong value,
                                // and this component is at index 1 (the "Hits" component), then we set up the hueristic helper struct to let us know that the next component (the Misses)
                                // needs to be overridden.
                                component_value                        = 0;
                                bool apply_cache_hit_counter_heuristic = counter_name.compare("L0CacheHit") == 0 || counter_name.compare("L1CacheHit") == 0 ||
                                                                         counter_name.compare("L2CacheHit") == 0;
                                if (apply_cache_hit_counter_heuristic && component_index == 1)
                                {
                                    // This is the "Hits" component which is derived from Requests - Misses.
                                    // If we need to clamp it to zero, then we need to adjust the next component (the Misses) to match the number of Requests
                                    counter_component_heuristic_helper.counter_index   = counter_index;
                                    counter_component_heuristic_helper.component_index = 0;
                                    counter_component_heuristic_helper.timestamp_index = timestamp_index;
                                    counter_component_heuristic_helper.use_heuristic   = true;
                                }
                            }
                        }
                        else
                        {
                            component_value = 0;
                        }
                    }
                }
            }
        }
    }
    else
    {
        // Non-derived counters do not have components -- return zero.
        component_value = 0;
    }
    return error_code;
}

const RawRgpSpmDataBase& RgpSpmDataBase::GetRawSpmDatabase() const
{
    return raw_spm_db_;
}

bool RgpSpmDataBase::IsTimestampValid(size_t timestamp_index) const
{
    bool is_valid = false;

    if (timestamp_index < mapped_timestamps_.size())
    {
        is_valid = mapped_timestamps_[timestamp_index].timestamp_is_valid;
    }

    return is_valid;
}

RgpErrorCode RgpSpmDataBase::OpenGPACounterContext()
{
    if (nullptr == gpa_counter_context_)
    {
        if (nullptr == gpa_func_table_)
        {
            gpa_func_table_ = GPUPerfAPICountersEntryPoints::Instance()->GetFuncTable();
            RGP_ASSERT(nullptr != gpa_func_table_);
            RGP_RETURN_ON_ERROR(nullptr != gpa_func_table_, kRgpErrorGpaFailure);
        }

        RGP_RETURN_ON_ERROR(GPUPerfAPICountersEntryPoints::Instance()->EntryPointsValid(), kRgpErrorGpaFailure);

        static const int kAmdVendorId = 0x1002;

        GpaCounterContextHardwareInfo counter_context_hardware_info = {};

        counter_context_hardware_info.device_id   = asic_info_.device_id;
        counter_context_hardware_info.revision_id = asic_info_.device_revision_id;
        counter_context_hardware_info.vendor_id   = kAmdVendorId;

        std::vector<GpaHardwareAttribute> hardware_attrs;
        GpaHardwareAttribute              hw_attr;

        int32_t num_sa   = asic_info_.shader_engines;
        int32_t num_cu   = asic_info_.shader_engines * asic_info_.compute_unit_per_shader_engine;
        int32_t num_simd = num_cu * asic_info_.simd_per_compute_unit;

        if (asic_info_.gfx_ip_level >= kRgpGfxLevel10)
        {
            num_sa *= 2;  // 2 SA per SE for gfx10 and newer.
            num_cu *= 2;  // compute_units_per_shader_engine is actually the number of WGPs.
        }

        hw_attr.gpa_hardware_attribute_type  = kGpaHardwareAttributeNumShaderEngines;
        hw_attr.gpa_hardware_attribute_value = asic_info_.shader_engines;
        hardware_attrs.push_back(hw_attr);
        hw_attr.gpa_hardware_attribute_type  = kGpaHardwareAttributeNumShaderArrays;
        hw_attr.gpa_hardware_attribute_value = num_sa;
        hardware_attrs.push_back(hw_attr);
        hw_attr.gpa_hardware_attribute_type  = kGpaHardwareAttributeNumComputeUnits;
        hw_attr.gpa_hardware_attribute_value = num_cu;
        hardware_attrs.push_back(hw_attr);
        hw_attr.gpa_hardware_attribute_type  = kGpaHardwareAttributeNumSimds;
        hw_attr.gpa_hardware_attribute_value = num_simd;
        hardware_attrs.push_back(hw_attr);
        hw_attr.gpa_hardware_attribute_type  = kGpaHardwareAttributeNumRenderBackends;
        hw_attr.gpa_hardware_attribute_value = 16;  // Hardcode until we can get from asicinfo -- this equals the number of CB or DB blocks.
        hardware_attrs.push_back(hw_attr);
        hw_attr.gpa_hardware_attribute_type  = kGpaHardwareAttributePeakVerticesPerClock;
        hw_attr.gpa_hardware_attribute_value = 8;  // Hardcode until we can get from asicinfo.
        hardware_attrs.push_back(hw_attr);
        hw_attr.gpa_hardware_attribute_type  = kGpaHardwareAttributePeakPrimitivesPerClock;
        hw_attr.gpa_hardware_attribute_value = 4;  // Hardcode until we can get from asicinfo.
        hardware_attrs.push_back(hw_attr);
        hw_attr.gpa_hardware_attribute_type  = kGpaHardwareAttributePeakPixelsPerClock;
        hw_attr.gpa_hardware_attribute_value = 4;  // Hardcode until we can get from asicinfo.
        hardware_attrs.push_back(hw_attr);

        counter_context_hardware_info.gpa_hardware_attribute_count = static_cast<GpaUInt32>(hardware_attrs.size());
        counter_context_hardware_info.gpa_hardware_attributes      = hardware_attrs.data();

        // NOTE: we are always using kGpaApiVulkan here. Since all officially-supported APIs are PAL-based, we need to
        // specify one of the PAL-based APIs. If we used the actual API, things would not be handled correctly for OpenCL,
        // which uses fewer counter block instances for per-SE blocks (it auto-sums them). Since the counter data is populated
        // by PAL, using Vulkan here makes sure that Vulkan, DX12, OpenCL and HIP are all handled correctly within GPA.
        // Also: we use Vulkan instead of DX12 because GPA doesn't support DX12 on Linux.
        // We may need to revisit this for non-PAL-based (i.e. GENERIC) APIs.
        GpaStatus gpa_status = gpa_func_table_->GpaCounterLibOpenCounterContext(kGpaApiVulkan,
                                                                                kGpaSessionSampleTypeDiscreteCounter,
                                                                                counter_context_hardware_info,
                                                                                kGpaOpenContextDefaultBit | kGpaOpenContextEnableHardwareCountersBit,
                                                                                &gpa_counter_context_);
        RGP_ASSERT(kGpaStatusOk == gpa_status);
        RGP_RETURN_ON_ERROR(kGpaStatusOk == gpa_status, kRgpErrorGpaFailure);
    }

    return kRgpOk;
}

RgpErrorCode RgpSpmDataBase::CalculateDerivedCounters(const ProgressCallback& progress_callback, const std::atomic_bool* should_abort)
{
    if (!derived_counters_calculated_)
    {
        derived_counters_calculated_ = true;
        if ((counter_info_1_2_supported_ || counter_info_2_0_supported_ || counter_info_3_0_supported_) && raw_spm_db_.number_of_timestamps > 0)
        {
            RgpErrorCode error_code = OpenGPACounterContext();
            RGP_RETURN_ON_ERROR(kRgpOk == error_code, error_code);

            if (nullptr != gpa_func_table_)
            {
                available_derived_counters_.clear();
                counter_value_is_bad_.clear();
                uint32_t number_of_skipped_counters        = 0;
                bool     apply_cache_hit_counter_heuristic = false;

                for (size_t z = 0; z < expected_derived_counters_.size(); z++)
                {
                    GpaUInt32       gpa_counter_index = 0;
                    GpaCounterParam counter_param;
                    counter_param.derived_counter_name = expected_derived_counters_[z].counter_info.gpa_counter_name.c_str();
                    counter_param.is_derived_counter   = true;
                    GpaStatus gpa_status = gpa_func_table_->GpaCounterLibGetCounterIndex(gpa_counter_context_, &counter_param, &gpa_counter_index);

                    if (kGpaStatusErrorCounterNotFound == gpa_status)
                    {
                        RgpPrint("Counter not found: {}", counter_param.derived_counter_name);
                        number_of_skipped_counters++;
                        // Skip this counter.
                        continue;
                    }

                    RGP_ASSERT_MESSAGE(gpa_status == kGpaStatusOk, "GPA Error: GetCounterIndex failed");
                    RGP_RETURN_ON_ERROR(gpa_status == kGpaStatusOk, kRgpErrorGpaFailure);

                    apply_cache_hit_counter_heuristic = strcmp(counter_param.derived_counter_name, "L0CacheHit") == 0 ||
                                                        strcmp(counter_param.derived_counter_name, "L1CacheHit") == 0 ||
                                                        strcmp(counter_param.derived_counter_name, "L2CacheHit") == 0;

                    const GpaCounterInfo* gpa_counter_info;
                    gpa_status = gpa_func_table_->GpaCounterLibGetCounterInfo(gpa_counter_context_, gpa_counter_index, &gpa_counter_info);
                    RGP_ASSERT_MESSAGE(gpa_status == kGpaStatusOk, "GPA Error: GetCounterInfo failed");

                    RGP_ASSERT_MESSAGE(nullptr != gpa_counter_info, "GPA Error: Null pointer");
                    if (nullptr == gpa_counter_info)
                    {
                        continue;  // Expected derived counter not found, skip it.
                    }
                    RGP_ASSERT_MESSAGE(gpa_counter_info->is_derived_counter, "GPA Error: Unexpected counter type");
                    RGP_ASSERT_MESSAGE(nullptr != gpa_counter_info->gpa_derived_counter, "GPA Error: Null pointer");
                    if (!gpa_counter_info->is_derived_counter || nullptr == gpa_counter_info->gpa_derived_counter)
                    {
                        continue;  // Unexpected counter found, skip it.
                    }
                    GpaUInt32 hw_counter_count = gpa_counter_info->gpa_derived_counter->gpa_hw_counter_count;
                    RGP_ASSERT_MESSAGE(hw_counter_count > 0, "GPA Error: No counters");
                    if (0 == hw_counter_count)
                    {
                        continue;  // Skip counters with no hardware counters.
                    }

                    std::vector<GpaUInt32> counter_offset(hw_counter_count, 0);
                    std::vector<GpaUInt32> counter_size_in_bytes(hw_counter_count, 0);
                    std::vector<GpaUInt64> raw_counter_values(hw_counter_count, 0);

                    static const uint32_t kNoOffsetForExtraGpaBlocks   = 0xFFFFFFFF;
                    static const size_t   kUnknownTimingCounterIndex   = 0xFFFFFFFF;
                    bool                  derived_counter_is_supported = true;
                    size_t                timing_counter_index         = kUnknownTimingCounterIndex;
                    uint32_t              previous_valid_gpa_block     = 0;
                    uint32_t              previous_valid_gpa_instance  = 0;

                    for (GpaUInt32 i = 0; i < hw_counter_count && derived_counter_is_supported; i++)
                    {
                        bool this_hardware_counter_found = false;

                        uint32_t gpa_block_id       = 0;
                        uint32_t gpa_block_instance = 0;
                        uint32_t gpa_event_index    = 0;

                        if (!gpa_counter_info->gpa_derived_counter->gpa_hw_counters[i].is_timing_block)
                        {
                            gpa_block_id       = static_cast<uint32_t>(gpa_counter_info->gpa_derived_counter->gpa_hw_counters[i].gpa_hw_block);
                            gpa_block_instance = gpa_counter_info->gpa_derived_counter->gpa_hw_counters[i].gpa_hw_block_instance;
                            gpa_event_index    = gpa_counter_info->gpa_derived_counter->gpa_hw_counters[i].gpa_hw_block_event_id;
                        }
                        else
                        {
                            RGP_ASSERT(timing_counter_index == kUnknownTimingCounterIndex);  // Ensure there's only one timing counter per derived counter.
                            if (timing_counter_index == kUnknownTimingCounterIndex)          // Ignore any additional timing counters (should never happen).
                            {
                                timing_counter_index        = i;
                                this_hardware_counter_found = true;
                            }
                        };

                        for (uint32_t j = 0; j < raw_spm_db_.number_of_spm_counter_info; j++)
                        {
                            uint32_t block_id       = 0;
                            uint32_t block_instance = 0;
                            uint32_t event_index    = 0;
                            if (counter_info_3_0_supported_)
                            {
                                SpmGpuBlock gpu_block = reinterpret_cast<SpmCounterDataHeader*>(raw_spm_db_.spm_counter_info)[j].gpu_block;
                                block_id              = static_cast<uint32_t>(gpu_block);

                                block_instance = reinterpret_cast<SpmCounterDataHeader*>(raw_spm_db_.spm_counter_info)[j].block_instance;
                                event_index    = reinterpret_cast<SpmCounterDataHeader*>(raw_spm_db_.spm_counter_info)[j].event_index;
                            }
                            else if (counter_info_2_0_supported_)
                            {
                                block_id       = reinterpret_cast<SpmCounterInfo_V2*>(raw_spm_db_.spm_counter_info)[j].gpu_block_id;
                                block_instance = reinterpret_cast<SpmCounterInfo_V2*>(raw_spm_db_.spm_counter_info)[j].gpu_block_instance;
                                event_index    = reinterpret_cast<SpmCounterInfo_V2*>(raw_spm_db_.spm_counter_info)[j].event_index;
                            }
                            else if (counter_info_1_2_supported_)
                            {
                                block_id       = reinterpret_cast<SpmCounterInfo_V1_2*>(raw_spm_db_.spm_counter_info)[j].gpu_block_id;
                                block_instance = reinterpret_cast<SpmCounterInfo_V1_2*>(raw_spm_db_.spm_counter_info)[j].gpu_block_instance;
                                event_index    = reinterpret_cast<SpmCounterInfo_V1_2*>(raw_spm_db_.spm_counter_info)[j].event_index;
                            }

                            if (block_id == gpa_block_id && block_instance == gpa_block_instance && event_index == gpa_event_index)
                            {
                                previous_valid_gpa_block    = gpa_block_id;
                                previous_valid_gpa_instance = gpa_block_instance;
                                this_hardware_counter_found = true;

                                if (counter_info_3_0_supported_)
                                {
                                    counter_offset[i]        = raw_spm_db_.counter_offsets[j];
                                    counter_size_in_bytes[i] = reinterpret_cast<SpmCounterDataHeader*>(raw_spm_db_.spm_counter_info)[j].data_size;
                                }
                                else if (counter_info_2_0_supported_)
                                {
                                    // divide stored data offset by 2 to translate from byte offset to word offset.
                                    counter_offset[i]        = reinterpret_cast<SpmCounterInfo_V2*>(raw_spm_db_.spm_counter_info)[j].data_offset / 2;
                                    counter_size_in_bytes[i] = reinterpret_cast<SpmCounterInfo_V2*>(raw_spm_db_.spm_counter_info)[j].data_size;
                                }
                                else if (counter_info_1_2_supported_)
                                {
                                    // divide stored data offset by 2 to translate from byte offset to word offset.
                                    counter_offset[i]        = reinterpret_cast<SpmCounterInfo_V1_2*>(raw_spm_db_.spm_counter_info)[j].data_offset / 2;
                                    counter_size_in_bytes[i] = 2;
                                }
                                break;
                            }
                        }

                        if (!this_hardware_counter_found)
                        {
                            if (gpa_block_id == previous_valid_gpa_block && gpa_block_instance == ++previous_valid_gpa_instance)
                            {
                                // In this case GPA might know about more instances than actually exist on the hardware.
                                // When this happens, the block_id will be the same as the previous block id and the instance
                                // will be one higher than the previous instance. In this situation, we act like we found the GPA
                                // counter (whose value will be zero). This mimics the behavior of GPA itself.
                                this_hardware_counter_found = true;
                                counter_offset[i]           = kNoOffsetForExtraGpaBlocks;
                            }
                        }

                        derived_counter_is_supported &= this_hardware_counter_found;
                    }

                    expected_derived_counters_[z].is_available = derived_counter_is_supported;

                    GpaUsageType counter_usage_type = kGpaUsageTypeLast;

                    if (derived_counter_is_supported)
                    {
                        const char* counter_desc;
                        gpa_status = gpa_func_table_->GpaCounterLibGetCounterDescription(gpa_counter_context_, gpa_counter_index, &counter_desc);
                        RGP_ASSERT_MESSAGE(gpa_status == kGpaStatusOk, "GPA Error: GetCounterDescription failed");
                        RGP_RETURN_ON_ERROR(gpa_status == kGpaStatusOk, kRgpErrorGpaFailure);

                        gpa_status = gpa_func_table_->GpaCounterLibGetCounterUsageType(gpa_counter_context_, gpa_counter_index, &counter_usage_type);
                        RGP_ASSERT_MESSAGE(gpa_status == kGpaStatusOk, "GPA Error: GetCounterUsageType failed");
                        RGP_RETURN_ON_ERROR(gpa_status == kGpaStatusOk, kRgpErrorGpaFailure);

                        // Ensure that our enum stays in sync with GPA.
                        static_assert(CounterUsageType::kCounterUsageTypeRatio == static_cast<CounterUsageType>(kGpaUsageTypeRatio),
                                      "Mismatch in Counter Usage Type with GPA");
                        static_assert(CounterUsageType::kCounterUsageTypePercentage == static_cast<CounterUsageType>(kGpaUsageTypePercentage),
                                      "Mismatch in Counter Usage Type with GPA");
                        static_assert(CounterUsageType::kCounterUsageTypeCycles == static_cast<CounterUsageType>(kGpaUsageTypeCycles),
                                      "Mismatch in Counter Usage Type with GPA");
                        static_assert(CounterUsageType::kCounterUsageTypeMilliseconds == static_cast<CounterUsageType>(kGpaUsageTypeMilliseconds),
                                      "Mismatch in Counter Usage Type with GPA");
                        static_assert(CounterUsageType::kCounterUsageTypeBytes == static_cast<CounterUsageType>(kGpaUsageTypeBytes),
                                      "Mismatch in Counter Usage Type with GPA");
                        static_assert(CounterUsageType::kCounterUsageTypeItems == static_cast<CounterUsageType>(kGpaUsageTypeItems),
                                      "Mismatch in Counter Usage Type with GPA");
                        static_assert(CounterUsageType::kCounterUsageTypeKilobytes == static_cast<CounterUsageType>(kGpaUsageTypeKilobytes),
                                      "Mismatch in Counter Usage Type with GPA");
                        static_assert(CounterUsageType::kCounterUsageTypeNanoseconds == static_cast<CounterUsageType>(kGpaUsageTypeNanoseconds),
                                      "Mismatch in Counter Usage Type with GPA");
                        static_assert(CounterUsageType::kCounterUsageTypeLast == static_cast<CounterUsageType>(kGpaUsageTypeLast),
                                      "Mismatch in Counter Usage Type with GPA");

                        // Component counters are derived counters whose values are used as "components" of other counters. Component counters are not added to the list of available counters.
                        if (!expected_derived_counters_[z].is_component_counter)
                        {
                            CounterInfo counter_info = {
                                expected_derived_counters_[z].counter_info, std::string(counter_desc), static_cast<CounterUsageType>(counter_usage_type)};
                            available_derived_counters_.push_back(counter_info);
                            // Initialize all counters as good.
                            counter_value_is_bad_.push_back(0);
                        }
                        else
                        {
                            CounterInfo counter_info = {
                                expected_derived_counters_[z].counter_info, std::string(counter_desc), static_cast<CounterUsageType>(counter_usage_type)};
                            available_counter_components_.push_back(counter_info);
                        }

                        std::vector<GpaFloat64> timestamps;
                        timestamps.resize(raw_spm_db_.number_of_timestamps);
                        counter_values_.push_back(timestamps);
                    }

                    uint32_t bad_value_count  = 0;
                    uint32_t good_value_count = 0;
                    for (uint32_t timestamp_index = 0; timestamp_index < raw_spm_db_.number_of_timestamps; timestamp_index++)
                    {
                        bool any_value_non_zero = false;
                        for (size_t i = 0; i < raw_counter_values.size(); i++)
                        {
                            if (kNoOffsetForExtraGpaBlocks == counter_offset[i])
                            {
                                raw_counter_values[i] = 0;
                            }
                            else if (i == timing_counter_index)
                            {
                                raw_counter_values[i] = raw_spm_db_.sampling_interval;
                                any_value_non_zero    = true;
                            }
                            else
                            {
                                GpaUInt32 this_counter_offset = counter_offset[i];
                                uint32_t  data_size_in_words  = counter_size_in_bytes[i];
                                if (data_size_in_words > 1)
                                {
                                    // Make sure the data size is even, since we are assuming all data sizes are multiples of 2 bytes.
                                    RGP_ASSERT(counter_size_in_bytes[i] % 2 == 0);
                                    data_size_in_words = counter_size_in_bytes[i] / 2;
                                    this_counter_offset *= data_size_in_words;
                                }
                                this_counter_offset += (timestamp_index * data_size_in_words);

                                // Now build up the actual counter value for cases where the counter is 32-bits (or larger).
                                uint32_t current_value = raw_spm_db_.counter_data[this_counter_offset];
                                while (data_size_in_words > 1)
                                {
                                    data_size_in_words--;
                                    uint16_t next_value = raw_spm_db_.counter_data[this_counter_offset + data_size_in_words];
                                    current_value       = current_value + (next_value << (data_size_in_words * 16));
                                }
                                raw_counter_values[i] = current_value;
                                any_value_non_zero |= raw_counter_values[i] != 0;
                            }
                        }

#ifdef DEBUG_DERIVED_SPM_COUNTER_DATA
                        {
                            std::stringstream timestamp_data;
                            timestamp_data << "CalculateDerived: Timestamp[" << timestamp_index << "]: " << mapped_timestamps_[timestamp_index]
                                           << " - Raw Data: ";

                            for (size_t i = 0; i < raw_counter_values.size(); i++)
                            {
                                timestamp_data << raw_counter_values[i] << ", ";
                            }

                            RgpPrint(timestamp_data.str().c_str());
                        }
#endif
                        GpaFloat64 derived_counter_value = 0;

                        if (derived_counter_is_supported)
                        {
                            if (any_value_non_zero)
                            {
                                gpa_status = gpa_func_table_->GpaCounterLibComputeDerivedCounterResult(
                                    gpa_counter_context_, gpa_counter_index, raw_counter_values.data(), hw_counter_count, &derived_counter_value);
                                RGP_ASSERT_MESSAGE(gpa_status == kGpaStatusOk, "GPA Error");
                                RGP_RETURN_ON_ERROR(gpa_status == kGpaStatusOk, kRgpErrorGpaFailure);

                                if (derived_counter_value < 0)
                                {
                                    if (apply_cache_hit_counter_heuristic)
                                    {
                                        // Heuristic to determine if value is truly "bad" or if it is "noise"
                                        // If the percentage is negative, but less than two percent, clamp it to zero and consider it "good".
                                        if (derived_counter_value < -2)
                                        {
                                            // If the percentage is negative, greater than two percent, but the number of Requests is "small",
                                            // such that the difference between Misses and Requests is less than 20, then we can clamp the value
                                            // to zero and consider it "good". This logic is based on the fact that the L*CacheHitCounters are
                                            // made up of Requests and Misses (each of which are half of the hw counter array).
                                            size_t    half_counter_count    = hw_counter_count / 2;
                                            GpaUInt64 this_counter_requests = 0;
                                            GpaUInt64 this_counter_misses   = 0;
                                            for (size_t i = 0; i < half_counter_count; i++)
                                            {
                                                this_counter_requests += raw_counter_values[i];
                                                this_counter_misses += raw_counter_values[i + half_counter_count];
                                            }

                                            if (this_counter_misses - this_counter_requests < 20)
                                            {
                                                // Value is "good" (and clamped to zero) based on small difference between Misses and Requests.
                                                derived_counter_value = 0;
                                            }
                                            else
                                            {
                                                bad_value_count++;
                                            }
                                        }
                                        else
                                        {
                                            // Value is "good" (and clamped to zero) since the difference is less than 2 percent.
                                            derived_counter_value = 0;
                                        }
                                    }
                                    else
                                    {
                                        bad_value_count++;
                                    }
                                }
                                else
                                {
                                    // If we find the first good value after 1 or 2 bad values, reset the bad count
                                    // because this may just be noise at the beginning. We only want to consider bad
                                    // values if there are more than two of them at the beginning or if any occur
                                    // after the first good value.
                                    if (bad_value_count <= 2 && good_value_count == 0)
                                    {
                                        bad_value_count = 0;
                                    }

                                    good_value_count++;
                                }

                                counter_values_[z - number_of_skipped_counters][timestamp_index] = derived_counter_value;
                            }
                            else
                            {
                                counter_values_[z - number_of_skipped_counters][timestamp_index] = 0;
                            }
                        }
                    }

                    if (filter_bad_spm_data_ && !expected_derived_counters_[z].is_component_counter && (bad_value_count > 0))
                    {
                        // If we've found a counter with bad data, mark its index as bad, and remove it
                        // from the list of available derived counters.
                        RgpPrint("Counter {} is bad", available_derived_counters_.back().counter_info.gpa_counter_name.c_str());
                        counter_value_is_bad_[z - number_of_skipped_counters] = 1;
                        available_derived_counters_.pop_back();
                    }

                    if (!derived_counter_is_supported)
                    {
                        number_of_skipped_counters++;
                    }

                    if (progress_callback)
                    {
                        progress_callback(expected_derived_counters_.size(), z + 1);
                    }

                    if (should_abort != nullptr && *should_abort)
                    {
                        return kRgpErrorAborted;
                    }
                }
            }
        }
    }

    return kRgpOk;
}

RgpErrorCode RgpSpmDataBase::GetCounterInfo(size_t counter_index, CounterQueryType query_type, bool derived, std::string& counter_info)
{
    RgpErrorCode error_code = kRgpErrorGpaFailure;

    if (query_type < kCounterQueryTypeCount)
    {
        if (derived)
        {
            error_code = CalculateDerivedCounters();
            if (kRgpOk == error_code)
            {
                RGP_ASSERT_MESSAGE(counter_index < available_derived_counters_.size(), "counter_index out of range");
                if (counter_index < available_derived_counters_.size())
                {
                    error_code = kRgpOk;
                    switch (query_type)
                    {
                    case kCounterQueryTypeName:
                        counter_info = available_derived_counters_[counter_index].counter_info.friendly_counter_name;
                        break;
                    case kCounterQueryTypeDescription:
                        counter_info = available_derived_counters_[counter_index].counter_desc;
                        break;
                    case kCounterQueryTypeGpaName:
                        counter_info = available_derived_counters_[counter_index].counter_info.gpa_counter_name;
                        break;
                    default:
                        error_code = kRgpErrorIndexOutOfRange;
                    }
                }
                else
                {
                    error_code = kRgpErrorIndexOutOfRange;
                }
            }
        }
        else
        {
            RGP_ASSERT_MESSAGE(counter_index < raw_spm_db_.number_of_spm_counter_info, "counter_index out of range");
            if (counter_index < raw_spm_db_.number_of_spm_counter_info)
            {
                if (counter_info_1_2_supported_ || counter_info_2_0_supported_ || counter_info_3_0_supported_)
                {
                    uint32_t gpu_block_id       = 0;
                    uint32_t gpu_block_instance = 0;
                    uint32_t event_index        = 0;
                    if (counter_info_3_0_supported_)
                    {
                        SpmGpuBlock gpu_block = reinterpret_cast<SpmCounterDataHeader*>(raw_spm_db_.spm_counter_info)[counter_index].gpu_block;
                        gpu_block_id          = static_cast<uint32_t>(gpu_block);

                        gpu_block_instance = reinterpret_cast<SpmCounterDataHeader*>(raw_spm_db_.spm_counter_info)[counter_index].block_instance;
                        event_index        = reinterpret_cast<SpmCounterDataHeader*>(raw_spm_db_.spm_counter_info)[counter_index].event_index;
                    }
                    else if (counter_info_2_0_supported_)
                    {
                        gpu_block_id       = reinterpret_cast<SpmCounterInfo_V2*>(raw_spm_db_.spm_counter_info)[counter_index].gpu_block_id;
                        gpu_block_instance = reinterpret_cast<SpmCounterInfo_V2*>(raw_spm_db_.spm_counter_info)[counter_index].gpu_block_instance;
                        event_index        = reinterpret_cast<SpmCounterInfo_V2*>(raw_spm_db_.spm_counter_info)[counter_index].event_index;
                    }
                    else if (counter_info_1_2_supported_)
                    {
                        gpu_block_id       = reinterpret_cast<SpmCounterInfo_V1_2*>(raw_spm_db_.spm_counter_info)[counter_index].gpu_block_id;
                        gpu_block_instance = reinterpret_cast<SpmCounterInfo_V1_2*>(raw_spm_db_.spm_counter_info)[counter_index].gpu_block_instance;
                        event_index        = reinterpret_cast<SpmCounterInfo_V1_2*>(raw_spm_db_.spm_counter_info)[counter_index].event_index;
                    }
                    error_code = OpenGPACounterContext();
                    if (kRgpOk == error_code)
                    {
                        if (nullptr != gpa_func_table_)
                        {
                            GpaUInt32       gpa_counter_index            = 0;
                            GpaCounterParam counter                      = {};
                            counter.is_derived_counter                   = false;
                            counter.gpa_hw_counter.gpa_hw_block          = static_cast<GpaHwBlock>(gpu_block_id);
                            counter.gpa_hw_counter.gpa_hw_block_instance = gpu_block_instance;
                            counter.gpa_hw_counter.gpa_hw_block_event_id = event_index;
                            counter.gpa_hw_counter.gpa_shader_mask       = kGpaShaderMaskAll;

                            GpaStatus gpa_status = gpa_func_table_->GpaCounterLibGetCounterIndex(gpa_counter_context_, &counter, &gpa_counter_index);

                            if (kGpaStatusOk == gpa_status)
                            {
                                switch (query_type)
                                {
                                case kCounterQueryTypeName:
                                case kCounterQueryTypeGpaName:
                                    const char* gpa_counter_name;
                                    gpa_status = gpa_func_table_->GpaCounterLibGetCounterName(gpa_counter_context_, gpa_counter_index, &gpa_counter_name);

                                    if (kGpaStatusOk == gpa_status)
                                    {
                                        counter_info = gpa_counter_name;
                                        error_code   = kRgpOk;
                                    }
                                    else
                                    {
                                        error_code = kRgpErrorGpaFailure;
                                    }
                                    break;
                                case kCounterQueryTypeDescription:
                                    const char* gpa_counter_desc;
                                    gpa_status =
                                        gpa_func_table_->GpaCounterLibGetCounterDescription(gpa_counter_context_, gpa_counter_index, &gpa_counter_desc);

                                    if (kGpaStatusOk == gpa_status)
                                    {
                                        counter_info = gpa_counter_desc;
                                        error_code   = kRgpOk;
                                    }
                                    else
                                    {
                                        error_code = kRgpErrorGpaFailure;
                                    }
                                    break;
                                default:
                                    error_code = kRgpErrorIndexOutOfRange;
                                }
                            }
                        }
                        else
                        {
                            error_code = kRgpErrorGpaFailure;
                        }

                        if (kRgpOk != error_code)
                        {
                            counter_info = std::to_string(gpu_block_id);
                            counter_info.append(":");
                            counter_info.append(std::to_string(gpu_block_instance));
                            counter_info.append(":");
                            counter_info.append(std::to_string(event_index));
                            error_code = kRgpOk;
                        }
                    }
                }
                else
                {
                    error_code = kRgpErrorUnsupportedLowSpecVersion;
                }
            }
            else
            {
                error_code = kRgpErrorIndexOutOfRange;
            }
        }
    }
    else
    {
        error_code = kRgpErrorIndexOutOfRange;
    }
    return error_code;
}
