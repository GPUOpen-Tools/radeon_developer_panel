// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  RGP file DerivedSpmDb chunk handling.

#include "counters/rgp_derived_spm_database.h"

#include <string.h>

#include <array>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "counters/rgp_rdf_derived_counter_chunks.h"
#include "counters/rgp_spm_database.h"

#include "rgp_file_format.h"
#include "trace_io.h"

#include "amdrdf.h"
#include "spm_db/derived_raw_builder.h"
#include "spm_db/derived_spm_database.h"
#include "spm_db/raw_spm_database.h"

/// @brief A structure encapsulating group information in the DerivedSpmDb chunk.
struct RgpFileDerivedSpmGroupInfo
{
    uint32_t size_in_bytes;             ///< The size in bytes of this RgpFileDerivedSpmGroupInfo struct instance.
    uint32_t offset;                    ///< The offset in bytes of the dynamic data (group_name).
    uint32_t group_name_length;         ///< The length in characters of the ASCII group name string.
    uint32_t group_description_length;  ///< The length in characters of the ASCII group description string.
    uint32_t number_of_counters;        ///< The number of counters in this group.
    // NOTE The following data follows:
    // char[group_name_length]        group_name;          ///< The name of the group.
    // char[group_description_length] group_description;   ///< The description of the group.
    // uint32_t[number_of_counters]   counter_index_list;  ///< An array of counter indices.
};

/// @brief A structure encapsulating counter information in the DerivedSpmDb chunk.
struct RgpFileDerivedSpmCounterInfo
{
    uint32_t size_in_bytes;               ///< The size in bytes of this RgpFileDerivedSpmCounterInfo struct instance.
    uint32_t offset;                      ///< The offset in bytes of the dynamic data (counter_name).
    uint32_t counter_name_length;         ///< The length in characters of the ASCII counter name string.
    uint32_t counter_description_length;  ///< The length in characters of the ASCII counter description string.
    uint32_t number_of_components;        ///< The number of components.
    uint32_t usage_type;                  ///< Corresponds to the GPA usage type.
    // NOTE The following data follows:
    // char[counter_name_length]        counter_name;          ///< The friendly name of the counter.
    // char[counter_description_length] counter_description;   ///< The description of the counter as provided by GPA.
    // uint32_t[number_of_components]   component_index_list;  ///< An array of component indices.
};

/// @brief A structure encapsulating counter component information in the DerivedSpmDb chunk.
struct RgpFileDerivedSpmComponentInfo
{
    uint32_t size_in_bytes;                 ///< The size in bytes of this RgpFileDerivedSpmComponentInfo struct instance.
    uint32_t offset;                        ///< The offset in bytes of the dynamic data (component_name).
    uint32_t component_name_length;         ///< The length in characters of the ASCII component name string.
    uint32_t component_description_length;  ///< The length in characters of the ASCII component description string.
    uint32_t usage_type;                    ///< Corresponds to the GPA usage type
    // NOTE The following data follows:
    // char[component_name_length]        component_name;        ///< The friendly name of the component.
    // char[component_description_length] component_description; ///< The description of the component as provided by GPA.
};

/// @brief A structure encapsulating the data in the DerivedSpmDb chunk
struct RgpFileChunkDerivedSpmDb
{
    RgpFileChunkHeader header;                ///< The chunk header.
    uint32_t           offset;                ///< The offset in bytes of the dynamic data (timestamps).
    uint32_t           flags;                 ///< Flags for future use.
    uint32_t           number_of_timestamps;  ///< The number of timestamps collected by RDP.
    uint32_t           number_of_groups;      ///< The number of counter groups.
    uint32_t           number_of_counters;    ///< The number of derived counters not including components.
    uint32_t           number_of_components;  ///< The number of counter components.
    uint32_t           sampling_interval;     ///< The sampling interval.
    // NOTE The following data follows:
    // uint64_t[number_of_timestamps]                       timestamps;       ///< An array holding the timestamp data.
    // RgpFileDerivedSpmGroupInfo[number_of_groups]         group_infos;      ///< An array holding all group information.
    // RgpFileDerivedSpmCounterInfo[number_of_counters]     counter_infos;    ///< An array holding all counter information.
    // RgpFileDerivedSpmComponentInfo[number_of_components] component_infos;  ///< An array holding all component information.
    // double[number_of_timestamps]                         counter_values;   ///< The counter values where length is number_of_counters * number_of_timestamps.
    // double[number_of_timestamps]                         component_values; ///< The counter values where length is number_of_components * number_of_timestamps.
};

/// @brief Helper function to read values from a byte array
/// @param [in,out] val Pointer to object to read into.
/// @param [in] read Pointer to byte array from which to read.
/// @param [in] count The number of values to read in (e.g. an array of values)
/// @return A pointer to the end of the read in the byte array.
template <typename T>
static const uint8_t* ReadValue(T* val, const uint8_t* read, uint32_t count = 1)
{
    memcpy(val, read, sizeof(T) * count);
    return (read + sizeof(T) * count);
}

/// @brief Helper function to write values to a byte array
/// @param [in] write Pointer to byte array to write.
/// @param [in,out] val Pointer to object that should be written (copied).
/// @param [in] count The number of values to read in (e.g. an array of values)
/// @return A pointer to the end of the write in the byte array.
template <typename T>
static uint8_t* WriteValue(uint8_t* write, const T* val, uint32_t count = 1)
{
    memcpy(write, val, sizeof(T) * count);
    return (write + sizeof(T) * count);
}

/// @brief Serializes a chunk header to the specified buffer.
///
/// @param [in] write Pointer representing the write buffer.
/// @param [in] header Header information to serialize.
///
/// @return A pointer to the end of the write.
static uint8_t* WriteChunkHeader(uint8_t* write, const RgpFileChunkHeader& header)
{
    // Write header
    write = WriteValue<uint32_t>(write, &header.chunk_identifier.value);
    write = WriteValue<int16_t>(write, &header.version_minor);
    write = WriteValue<int16_t>(write, &header.version_major);
    write = WriteValue<int32_t>(write, &header.size_in_bytes);
    write = WriteValue<int32_t>(write, &header.padding);

    return write;
}

/// @brief Deserializes a chunk header.
///
/// @param [out] header The deserialized chunk header
/// @param [in] read A pointer to the beginning of a serialized chunk header.
///
/// @return A pointer to the end of the read.
static const uint8_t* ReadChunkHeader(RgpFileChunkHeader& header, const uint8_t* read)
{
    // Read header
    read = ReadValue<uint32_t>(&header.chunk_identifier.value, read);
    read = ReadValue<int16_t>(&header.version_minor, read);
    read = ReadValue<int16_t>(&header.version_major, read);
    read = ReadValue<int32_t>(&header.size_in_bytes, read);
    read = ReadValue<int32_t>(&header.padding, read);

    return read;
}

/// @brief Serializes a DerivedSpmGroupInfo struct to the specified buffer.
///
/// @param [in] write Pointer to the buffer to serialize data into.
/// @param [in] group Group to serialize.
/// @param [in] counter_index_map A map from counters to their serialized index.
///
/// @return A pointer to the end of the written data.
static uint8_t* WriteDerivedSpmGroupInfo(uint8_t*                                                              write,
                                         const spm_db::DerivedSpmGroup&                                        group,
                                         const std::unordered_map<const spm_db::DerivedSpmCounter*, uint32_t>& counter_index_map)
{
    uint32_t name_length   = static_cast<uint32_t>(group.Name().length());
    uint32_t desc_length   = static_cast<uint32_t>(group.Description().length());
    uint32_t num_members   = static_cast<uint32_t>(group.Members().size());
    uint32_t header_length = sizeof(RgpFileDerivedSpmGroupInfo);
    uint32_t total_length  = sizeof(RgpFileDerivedSpmGroupInfo) + name_length + desc_length + (num_members * sizeof(uint32_t));

    // Write fixed data
    write = WriteValue<uint32_t>(write, &total_length);
    write = WriteValue<uint32_t>(write, &header_length);
    write = WriteValue<uint32_t>(write, &name_length);
    write = WriteValue<uint32_t>(write, &desc_length);
    write = WriteValue<uint32_t>(write, &num_members);
    // Write dynamic data
    write = WriteValue<char>(write, group.Name().c_str(), name_length);
    write = WriteValue<char>(write, group.Description().c_str(), desc_length);

    std::vector<uint32_t> member_index_list;
    for (auto member : group.Members())
    {
        member_index_list.push_back(counter_index_map.at(member));
    }
    write = WriteValue<uint32_t>(write, member_index_list.data(), num_members);

    return write;
}

/// @brief Serializes a DerivedSpmCounterInfo struct to the specified buffer.
///
/// @param [in] write Pointer representing the write buffer.
/// @param [in] counter Counter information to serialize.
/// @param [in] counter_index_map A map from counters to their serialized index.
///
/// @return A pointer to the end of the write.
static uint8_t* WriteDerivedSpmCounterInfo(uint8_t*                                                              write,
                                           const spm_db::DerivedSpmCounter&                                      counter,
                                           const std::unordered_map<const spm_db::DerivedSpmCounter*, uint32_t>& counter_index_map)
{
    uint32_t name_length    = static_cast<uint32_t>(counter.DisplayName().length());
    uint32_t desc_length    = static_cast<uint32_t>(counter.Description().length());
    uint32_t num_components = static_cast<uint32_t>(counter.Components().size());
    uint32_t header_length  = sizeof(RgpFileDerivedSpmCounterInfo);
    uint32_t total_length   = sizeof(RgpFileDerivedSpmCounterInfo) + name_length + desc_length + (num_components * sizeof(uint32_t));

    // Write fixed data
    write = WriteValue<uint32_t>(write, &total_length);
    write = WriteValue<uint32_t>(write, &header_length);
    write = WriteValue<uint32_t>(write, &name_length);
    write = WriteValue<uint32_t>(write, &desc_length);
    write = WriteValue<uint32_t>(write, &num_components);

    uint32_t usage_type = static_cast<uint32_t>(counter.UsageType());
    write               = WriteValue<uint32_t>(write, &usage_type);

    // Write dynamic data
    write = WriteValue<char>(write, counter.DisplayName().c_str(), name_length);
    write = WriteValue<char>(write, counter.Description().c_str(), desc_length);

    std::vector<uint32_t> component_index_list;
    for (auto component : counter.Components())
    {
        component_index_list.push_back(counter_index_map.at(component.counter));
    }
    write = WriteValue<uint32_t>(write, component_index_list.data(), num_components);
    return write;
}

/// @brief Serializes a DerivedSpmComponentInfo struct to the specified buffer.
///
/// @param [in] write Pointer representing the write buffer.
/// @param [in] counter Component information to serialize.
///
/// @return A pointer to the end of the write.
static uint8_t* WriteDerivedSpmComponentInfo(uint8_t* write, const spm_db::DerivedSpmCounter& counter)

{
    uint32_t name_length   = static_cast<uint32_t>(counter.DisplayName().length());
    uint32_t desc_length   = static_cast<uint32_t>(counter.Description().length());
    uint32_t header_length = sizeof(RgpFileDerivedSpmComponentInfo);
    uint32_t total_length  = sizeof(RgpFileDerivedSpmComponentInfo) + name_length + desc_length;

    // Write fixed data
    write = WriteValue<uint32_t>(write, &total_length);
    write = WriteValue<uint32_t>(write, &header_length);
    write = WriteValue<uint32_t>(write, &name_length);
    write = WriteValue<uint32_t>(write, &desc_length);

    uint32_t usage_type = static_cast<uint32_t>(counter.UsageType());
    write               = WriteValue<uint32_t>(write, &usage_type);

    // Write dynamic data
    write = WriteValue<char>(write, counter.DisplayName().c_str(), name_length);
    write = WriteValue<char>(write, counter.Description().c_str(), desc_length);

    return write;
}

struct SpmTiming
{
    uint32_t              sample_interval;
    std::vector<uint64_t> timestamps;
};

/// @brief Serializes a derived SPM DB as a legacy derived SPM DB file chunk.
///
/// @param [in] derived_spm_db The derived SPM DB to serialize.
/// @param [in] timing SPM timing data to serialize as part of the derived SPM file chunk.
///
/// @return The serialized chunk as a byte array.
static std::vector<uint8_t> SerializeChunkDerivedSpmDb(const spm_db::DerivedSpmDataBase& derived_spm_db, const SpmTiming& timing)
{
    size_t chunk_size_bytes = sizeof(RgpFileChunkDerivedSpmDb)                                                      // header
                              + timing.timestamps.size() * sizeof(uint64_t)                                         // timestamps
                              + (derived_spm_db.Counters().size() * derived_spm_db.NumSamples() * sizeof(double));  // sample data
    // Additional bytes for group and counter metadata are added later.

    // Go through all counters to build a set of counters that are components of other counters.
    std::unordered_set<const spm_db::DerivedSpmCounter*> component_counters;  // The set of counters that are components of other counters.
    for (const auto& name_counter_pair : derived_spm_db.Counters())
    {
        const spm_db::DerivedSpmCounter& counter = name_counter_pair.second;
        for (const spm_db::DerivedSpmCounter::Component& component : counter.Components())
        {
            component_counters.insert(component.counter);
        }
    }

    // Ensure that no component counters contain components themselves,
    // as we cannot represent that in the legacy format.
    // If such a counter is found, abort and return the empty vector.
    for (const spm_db::DerivedSpmCounter* component_counter : component_counters)
    {
        if (!component_counter->Components().empty())
        {
            return std::vector<uint8_t>{};
        }
    }

    // Iterate through the derived SPM DB's counters to generate arbitrary orderings
    // of counters and components for serialization,
    // and compute necessary space for serialization of counter metadata.

    // A map from a counter to its index in either the counters or the components, depending on whether it is in component_counters.
    std::unordered_map<const spm_db::DerivedSpmCounter*, uint32_t> counter_index_map;
    uint32_t                                                       next_non_component_idx = 0;
    uint32_t                                                       next_component_idx     = 0;
    for (const auto& name_counter_pair : derived_spm_db.Counters())
    {
        const spm_db::DerivedSpmCounter& counter = name_counter_pair.second;
        if (component_counters.find(&counter) == component_counters.end())
        {
            counter_index_map.insert({&counter, next_non_component_idx});
            next_non_component_idx++;
            chunk_size_bytes += sizeof(RgpFileDerivedSpmCounterInfo) + counter.DisplayName().length() + counter.Description().length() +
                                (counter.Components().size() * sizeof(uint32_t));
        }
        else
        {
            counter_index_map.insert({&counter, next_component_idx});
            next_component_idx++;
            chunk_size_bytes += sizeof(RgpFileDerivedSpmComponentInfo) + +counter.DisplayName().length() + counter.Description().length();
        }
    }

    // Iterate through groups to compute necessary space for serialization.
    for (const spm_db::DerivedSpmGroup& group : derived_spm_db.Groups())
    {
        chunk_size_bytes +=
            sizeof(RgpFileDerivedSpmGroupInfo) + group.Name().length() + group.Description().length() + (group.Members().size() * sizeof(uint32_t));
    }

    std::vector<uint8_t> buffer(chunk_size_bytes, 0);
    uint8_t*             write = buffer.data();

    // Write header
    RgpFileChunkIdentifier id;
    id.chunk_type  = kRgpFileChunkTypeDerivedSpmDatabase;
    id.chunk_index = 0;
    id.reserved    = 0;
    RgpFileChunkHeader chunk_header{id, 0, 0, static_cast<int32_t>(chunk_size_bytes), 0};
    write = WriteChunkHeader(write, chunk_header);

    // Write fixed data
    uint32_t derived_spm_header_length = sizeof(RgpFileChunkDerivedSpmDb);
    uint32_t flags                     = 0;
    uint32_t num_timestamps            = static_cast<uint32_t>(timing.timestamps.size());
    uint32_t num_groups                = static_cast<uint32_t>(derived_spm_db.Groups().size());

    write = WriteValue<uint32_t>(write, &derived_spm_header_length);
    write = WriteValue<uint32_t>(write, &flags);
    write = WriteValue<uint32_t>(write, &num_timestamps);
    write = WriteValue<uint32_t>(write, &num_groups);
    write = WriteValue<uint32_t>(write, &next_non_component_idx);
    write = WriteValue<uint32_t>(write, &next_component_idx);
    write = WriteValue<uint32_t>(write, &timing.sample_interval);

    // Write dynamic data
    // Timestamps[]
    write = WriteValue<uint64_t>(write, timing.timestamps.data(), static_cast<uint32_t>(timing.timestamps.size()));
    // DerivedSpmGroupInfo[]
    for (const spm_db::DerivedSpmGroup& group : derived_spm_db.Groups())
    {
        write = WriteDerivedSpmGroupInfo(write, group, counter_index_map);
    }
    // DerivedSpmCounterInfo[]
    for (const auto& name_counter_pair : derived_spm_db.Counters())
    {
        const spm_db::DerivedSpmCounter& counter = name_counter_pair.second;
        if (component_counters.find(&counter) == component_counters.end())
        {
            write = WriteDerivedSpmCounterInfo(write, counter, counter_index_map);
        }
    }

    // DerivedSpmComponentInfo[]
    for (const auto& name_counter_pair : derived_spm_db.Counters())
    {
        const spm_db::DerivedSpmCounter& counter = name_counter_pair.second;
        if (component_counters.find(&counter) != component_counters.end())
        {
            write = WriteDerivedSpmComponentInfo(write, counter);
        }
    }

    // CounterData[]
    for (const auto& name_counter_pair : derived_spm_db.Counters())
    {
        const spm_db::DerivedSpmCounter& counter = name_counter_pair.second;
        if (component_counters.find(&counter) == component_counters.end())
        {
            write = WriteValue<double>(write, counter.Samples().data(), derived_spm_db.NumSamples());
        }
    }

    // ComponentData[]
    for (const auto& name_counter_pair : derived_spm_db.Counters())
    {
        const spm_db::DerivedSpmCounter& counter = name_counter_pair.second;
        if (component_counters.find(&counter) != component_counters.end())
        {
            write = WriteValue<double>(write, counter.Samples().data(), derived_spm_db.NumSamples());
        }
    }

    return buffer;
}

/// @brief Finds the the first instance of a certain chunk type in an .rgp file.
///
/// @param [out] chunk_start A pointer into the file buffer to the beginning of the first desired chunk.
/// @param [in] file_start A pointer to the beginning of the .rgp file as a byte array.
/// @param [in] file_size The size in bytes of the .rgp file.
/// @param [in] type The desired chunk type.
///
/// @retval kRgpFileWriterStatusOk          The operation completed successfully.
/// @retval kRgpFileWriterStatusParserError There was an error parsing the buffer.
static RgpFileWriterStatus GetPointerToFirstChunkWithType(const uint8_t** chunk_start, const uint8_t* file_start, size_t file_size, RgpFileChunkType type)
{
    if (nullptr == file_start)
    {
        // Error: invalid input
        return kRgpFileWriterStatusParserError;
    }

    int32_t chunk_offset;
    memcpy(&chunk_offset, file_start + sizeof(RgpFileChunkHeader), sizeof(int32_t));

    // Move our pointer to the first chunk header
    const uint8_t* header_start = file_start + chunk_offset;

    // Loop through the headers until we find the desired type.
    // Our loop condition is whether we have passed the end of the file
    RgpFileChunkHeader current_header;
    while (static_cast<size_t>(header_start - file_start) < file_size)
    {
        ReadChunkHeader(current_header, header_start);
        if (static_cast<uint32_t>(current_header.size_in_bytes) < sizeof(RgpFileChunkHeader))
        {
            // Error in data if the size of a chunk is less than the header size.
            // We need this check to make sure we don't get stuck.
            return kRgpFileWriterStatusParserError;
        }
        else if (current_header.chunk_identifier.chunk_type == type)
        {
            // We found the chunk we were looking for, return it.
            *chunk_start = header_start;
            return kRgpFileWriterStatusOk;
        }

        // Otherwise move our pointer to the start of the next chunk
        header_start += current_header.size_in_bytes;
    }
    // We got to the end of the file and could not find the desired chunk
    return kRgpFileWriterStatusParserError;
}

/// @brief Deserializes a RgpFileChunkAsicInfo struct from a specified buffer.
///
/// @param [in] read A pointer to the beginning of the serialized RgpFileChunkAsicInfo.
///
/// @return The deserialized RgpFileChunkAsicInfo struct.
static RgpFileChunkAsicInfo DeserializeChunkAsicInfo(const uint8_t* read)
{
    RgpFileChunkAsicInfo chunk;
    read = ReadChunkHeader(chunk.header, read);
    read = ReadValue<uint64_t>(&chunk.flags, read);
    read = ReadValue<int64_t>(&chunk.shader_core_clock, read);
    read = ReadValue<int64_t>(&chunk.memory_clock, read);
    read = ReadValue<int32_t>(&chunk.device_id, read);
    read = ReadValue<int32_t>(&chunk.device_revision_id, read);
    read = ReadValue<int32_t>(&chunk.vgprs_per_simd, read);
    read = ReadValue<int32_t>(&chunk.sgprs_per_simd, read);
    read = ReadValue<int32_t>(&chunk.shader_engines, read);
    read = ReadValue<int32_t>(&chunk.compute_unit_per_shader_engine, read);
    read = ReadValue<int32_t>(&chunk.simd_per_compute_unit, read);
    read = ReadValue<int32_t>(&chunk.wavefronts_per_simd, read);
    read = ReadValue<int32_t>(&chunk.minimum_vgpr_alloc, read);
    read = ReadValue<int32_t>(&chunk.vgpr_alloc_granularity, read);
    read = ReadValue<int32_t>(&chunk.minimum_sgpr_alloc, read);
    read = ReadValue<int32_t>(&chunk.sgpr_alloc_granularity, read);
    read = ReadValue<int32_t>(&chunk.hardware_contexts, read);
    read = ReadValue<int32_t>(&chunk.gpu_type, read);
    read = ReadValue<int32_t>(&chunk.gfx_ip_level, read);
    read = ReadValue<int32_t>(&chunk.gpu_index, read);
    read = ReadValue<int32_t>(&chunk.gds_size, read);
    read = ReadValue<int32_t>(&chunk.gds_per_shader_engine_size, read);
    read = ReadValue<int32_t>(&chunk.ce_ram_size, read);
    read = ReadValue<int32_t>(&chunk.ce_ram_size_graphics, read);
    read = ReadValue<int32_t>(&chunk.ce_ram_size_compute, read);
    read = ReadValue<int32_t>(&chunk.maximum_dedicated_cu, read);
    read = ReadValue<uint64_t>(&chunk.vram_size, read);
    read = ReadValue<int32_t>(&chunk.vram_bus_width, read);
    read = ReadValue<int32_t>(&chunk.level2_cache_size, read);
    read = ReadValue<int32_t>(&chunk.level1_cache_size, read);
    read = ReadValue<int32_t>(&chunk.lds_size, read);
    read = ReadValue<char>(chunk.gpu_name, read, kRgpMaxGpuNameInFile);
    read = ReadValue<float>(&chunk.alu_per_clock, read);
    read = ReadValue<float>(&chunk.textures_per_clock, read);
    read = ReadValue<float>(&chunk.primitives_per_clock, read);
    read = ReadValue<float>(&chunk.pixels_per_clock, read);
    read = ReadValue<uint64_t>(&chunk.gpu_timestamp_frequency, read);
    read = ReadValue<uint64_t>(&chunk.max_shader_core_clock, read);
    read = ReadValue<uint64_t>(&chunk.max_memory_clock, read);
    // Chunk version 0.1
    if (chunk.header.version_minor >= 1)
    {
        read = ReadValue<uint32_t>(&chunk.memory_ops_per_clock, read);
        read = ReadValue<uint32_t>(&chunk.memory_chip_type, read);
    }
    // Chunk version 0.2
    if (chunk.header.version_minor >= 2)
    {
        read = ReadValue<uint32_t>(&chunk.lds_allocation_granularity, read);
    }
    // Chunk version 0.3
    if (chunk.header.version_minor >= 3)
    {
        read = ReadValue<char>(chunk.active_cu_mask, read, kRgpActiveCuMaskLenInFileBytes);
        read = ReadValue<char>(chunk.active_cu_mask_reserved, read, kRgpActiveCuMaskLenInFileBytes);
    }
    // Chunk version 0.5
    if (chunk.header.version_minor >= 5)
    {
        read = ReadValue<char>(chunk.active_pixel_packer_mask, read, kRgpActivePixelPackerMaskLenInFileBytes);
        read = ReadValue<char>(chunk.active_pixel_packer_mask_reserved, read, kRgpActivePixelPackerMaskLenInFileBytes);
        read = ReadValue<int32_t>(&chunk.gl1_cache_size, read);
        read = ReadValue<int32_t>(&chunk.instruction_cache_size, read);
        read = ReadValue<int32_t>(&chunk.scalar_cache_size, read);
        read = ReadValue<int32_t>(&chunk.mall_cache_size, read);
    }

    return chunk;
}

static constexpr const char* kAsicInfoChunkId = "AsicInfo";

static RgpFileWriterStatus DeserializeChunkAsicInfo(rdfChunkFile* chunk_file, RgpFileChunkAsicInfoRdfV3& chunk)
{
    int64_t asic_info_chunk_count;
    if (rdfChunkFileGetChunkCount(chunk_file, kAsicInfoChunkId, &asic_info_chunk_count) != rdfResultOk || asic_info_chunk_count == 0)
    {
        return kRgpFileWriterStatusFileReadError;
    }

    int64_t chunk_size{};
    if (rdfChunkFileGetChunkDataSize(chunk_file, kAsicInfoChunkId, 0, &chunk_size) != rdfResultOk)
    {
        return kRgpFileWriterStatusFileReadError;
    }

    uint32_t version;
    if (rdfChunkFileGetChunkVersion(chunk_file, kAsicInfoChunkId, 0, &version) != rdfResultOk)
    {
        return kRgpFileWriterStatusFileReadError;
    }

    int64_t desired_size = 0;
    switch (version)
    {
    case 2:
        desired_size = sizeof(RgpFileChunkAsicInfoRdfV2);
        break;
    case 3:
        desired_size = sizeof(RgpFileChunkAsicInfoRdfV3);
        break;
    default:
        return kRgpFileWriterStatusFileReadError;
    }

    if (chunk_size != desired_size)
    {
        return kRgpFileWriterStatusFileReadError;
    }

    if (rdfChunkFileReadChunkData(chunk_file, kAsicInfoChunkId, 0, &chunk) != rdfResultOk)
    {
        return kRgpFileWriterStatusFileReadError;
    }

    return kRgpFileWriterStatusOk;
}

/// @brief Load a raw SPM DB from a legacy RGP file chunk.
///
/// @param [in] spm_db_start A buffer containing a serialized raw SPM DB RGP chunk.
///
/// @retval
/// A pair containing loaded SPM timing data and raw SPM counter data on success.
/// @retval
/// std::nullopt on failure.
static std::optional<std::pair<SpmTiming, spm_db::RawSpmDataBase>> LoadRawSpmLegacy(const uint8_t* spm_db_start)
{
    // When the SPM chunk added the event index to SpmCounterInfo (v1.2)
    constexpr int kRgpFileChunkTypeSpmDbAddedEventIndexMajorVersion = 1;  ///< Major version of the SPM Db chunk when the event index was added.
    constexpr int kRgpFileChunkTypeSpmDbAddedEventIndexMinorVersion = 2;  ///< Minor version of the SPM Db chunk when the event index was added.

    // When the SPM chunk added the sampling interval (v1.3)
    constexpr int kRgpFileChunkTypeSpmDbAddedSamplingIntervalMajorVersion = 1;  ///< Major version of the Spm Db chunk when the sampling interval was added.
    constexpr int kRgpFileChunkTypeSpmDbAddedSamplingIntervalMinorVersion = 3;  ///< Minor version of the Spm Db chunk when the sampling interval was added.

    const RgpFileChunkHeader* current_file_chunk = reinterpret_cast<const RgpFileChunkHeader*>(spm_db_start);

    // Read data from SPM DB chunk header, depending on the chunk version.
    bool           counter_info_1_2_supported = false;
    bool           counter_info_2_0_supported = false;
    uint32_t       sampling_interval          = 0;
    uint32_t       num_counters               = 0;
    uint32_t       num_timestamps             = 0;
    const uint8_t* payload_start              = nullptr;
    if (current_file_chunk->version_major >= kRgpFileChunkTypeSpmDbSizeMembersAddedMajorVersion)
    {
        const RgpFileChunkSpmDb* spm_db_chunk = reinterpret_cast<const RgpFileChunkSpmDb*>(current_file_chunk);
        counter_info_2_0_supported            = true;
        sampling_interval                     = spm_db_chunk->sampling_interval;
        payload_start                         = reinterpret_cast<const unsigned char*>(spm_db_chunk) + spm_db_chunk->preamble_size;
        num_counters                          = spm_db_chunk->number_of_spm_counter_info;
        num_timestamps                        = spm_db_chunk->number_of_timestamps;
    }
    else
    {
        counter_info_1_2_supported = current_file_chunk->version_major > kRgpFileChunkTypeSpmDbAddedEventIndexMajorVersion ||
                                     (current_file_chunk->version_major == kRgpFileChunkTypeSpmDbAddedEventIndexMajorVersion &&
                                      current_file_chunk->version_minor >= kRgpFileChunkTypeSpmDbAddedEventIndexMinorVersion);
        if (!counter_info_1_2_supported)
        {
            // We do not support pre-1.2 raw SPM chunk. If this chunk's version is too low, skip it.
            return std::nullopt;
        }

        const RgpFileChunkSpmDbV1* spm_db_chunk = reinterpret_cast<const RgpFileChunkSpmDbV1*>(current_file_chunk);
        num_timestamps                          = spm_db_chunk->number_of_timestamps;
        num_counters                            = spm_db_chunk->number_of_spm_counter_info;
        // Prior to v1.3, there is no sampling interval in the chunk, so we assume 4096, the value used by default,
        // and adjust the payload start address accordingly.
        bool sampling_interval_1_3_supported = current_file_chunk->version_major > kRgpFileChunkTypeSpmDbAddedSamplingIntervalMajorVersion ||
                                               (current_file_chunk->version_major == kRgpFileChunkTypeSpmDbAddedSamplingIntervalMajorVersion &&
                                                current_file_chunk->version_minor >= kRgpFileChunkTypeSpmDbAddedSamplingIntervalMinorVersion);
        if (sampling_interval_1_3_supported)
        {
            sampling_interval = spm_db_chunk->sampling_interval;
            payload_start     = reinterpret_cast<const uint8_t*>(spm_db_chunk) + sizeof(RgpFileChunkSpmDbV1);
        }
        else
        {
            sampling_interval = 4096;
            payload_start     = reinterpret_cast<const uint8_t*>(spm_db_chunk) + sizeof(RgpFileChunkSpmDbV1) - sizeof(RgpFileChunkSpmDbV1::sampling_interval);
        }
    }

    // Read timestamps.
    std::vector<uint64_t> timestamps     = std::vector<uint64_t>(num_timestamps, 0);
    const uint64_t*       timestamps_src = reinterpret_cast<const uint64_t*>(payload_start);
    memcpy(timestamps.data(), timestamps_src, num_timestamps * sizeof(uint64_t));

    // Get the size of the timestamp data for navigation of the chunk structure.
    const size_t size_of_timestamp_data = num_timestamps * sizeof(uint64_t);

    // Initialize the SPM DB we allocated earlier with the timestamps we read.
    // We only have valid event indices in counter info values for version 1.2+ of RGP's counter info format.
    spm_db::RawSpmDataBase spm_db(num_timestamps);

    // Read counter metadata and the sample data for each counter depending on the chunk version.
    for (uint32_t i = 0; i < num_counters; i++)
    {
        if (counter_info_2_0_supported)
        {
            const SpmCounterInfo_V2* counter_infos_src = reinterpret_cast<const SpmCounterInfo_V2*>(payload_start + size_of_timestamp_data);

            // Get the metadata for this counter.
            const SpmCounterInfo_V2& counter_info = counter_infos_src[i];

            uint32_t sample_data_offset = counter_info.data_offset;  // The offset of the start of the sample data for this counter.
            uint32_t data_size          = counter_info.data_size;    // The size of a single sample for this counter.

            // Read counter data: set up a buffer for this counter's samples appropriate for their size,
            // read data into it, and transfer the loaded counter to the DB.
            switch (data_size)
            {
            case 2:
            {
                std::vector<uint16_t> counter_samples = std::vector<uint16_t>(num_timestamps, 0);
                memcpy(counter_samples.data(), payload_start + sample_data_offset, data_size * num_timestamps);
                spm_db.AddCounter(
                    static_cast<GpaHwBlock>(counter_info.gpu_block_id), counter_info.gpu_block_instance, counter_info.event_index, std::move(counter_samples));
            }
            break;
            case 4:
            {
                std::vector<uint32_t> counter_samples = std::vector<uint32_t>(num_timestamps, 0);
                memcpy(counter_samples.data(), payload_start + sample_data_offset, data_size * num_timestamps);
                spm_db.AddCounter(
                    static_cast<GpaHwBlock>(counter_info.gpu_block_id), counter_info.gpu_block_instance, counter_info.event_index, std::move(counter_samples));
            }
            break;
            default:
                return std::nullopt;
            }
        }
        else
        {
            const SpmCounterInfo_V1_2* counter_infos_src = reinterpret_cast<const SpmCounterInfo_V1_2*>(payload_start + size_of_timestamp_data);

            // Get the metadata for this counter.
            const SpmCounterInfo_V1_2& counter_info = counter_infos_src[i];

            // The offset of the start of the sample data for this counter.
            uint32_t sample_data_offset = counter_info.data_offset;

            // Read counter data
            std::vector<uint16_t> counter_samples = std::vector<uint16_t>(num_timestamps, 0);
            memcpy(counter_samples.data(), payload_start + sample_data_offset, sizeof(uint16_t) * num_timestamps);
            spm_db.AddCounter(
                static_cast<GpaHwBlock>(counter_info.gpu_block_id), counter_info.gpu_block_instance, counter_info.event_index, std::move(counter_samples));
        }
    }

    return std::make_pair(SpmTiming{sampling_interval, std::move(timestamps)}, std::move(spm_db));
}
static bool isGfx10OrHigher(const RgpFileChunkAsicInfoRdfV2& asic_parameters)
{
    return asic_parameters.gfx_ip_level.major >= 10;
}

static bool isGfx10OrHigher(const RgpFileChunkAsicInfo& asic_parameters)
{
    return asic_parameters.gfx_ip_level >= kRgpGfxLevel10;
}

/// @brief Open a GPA counter valid for given hardware.
///
/// @tparam T The type containing hardware info. Either RgpFileChunkAsicInfoRdfV2 or RgpFileChunkAsicInfo.
/// @param [in] asic_parameters A description of the hardware to open the counter context for.
///
/// @return A GPA counter context on success, or nullptr on failure.
template <typename T>
static GpaCounterContext OpenGPACounterContext(const T& asic_parameters)
{
    if (!GPUPerfAPICountersEntryPoints::Instance()->EntryPointsValid())
    {
        return nullptr;
    }

    GpaCounterLibFuncTable* gpa_func_table = GPUPerfAPICountersEntryPoints::Instance()->GetFuncTable();
    assert(nullptr != gpa_func_table);

    constexpr int kAmdVendorId = 0x1002;

    GpaCounterContextHardwareInfo counter_context_hardware_info = {};

    counter_context_hardware_info.device_id   = asic_parameters.device_id;
    counter_context_hardware_info.revision_id = asic_parameters.device_revision_id;
    counter_context_hardware_info.vendor_id   = kAmdVendorId;

    uint32_t num_sa = asic_parameters.shader_engines;
    uint32_t num_cu = asic_parameters.shader_engines * asic_parameters.compute_unit_per_shader_engine;

    if (isGfx10OrHigher(asic_parameters))
    {
        num_sa *= 2;  // 2 SA per SE for gfx10 and newer.
        num_cu *= 2;  // compute_units_per_shader_engine is actually the number of WGPs.
    }

    const uint32_t num_simd                 = num_cu * asic_parameters.simd_per_compute_unit;
    const uint32_t num_wave_fronts_per_simd = asic_parameters.wavefronts_per_simd;
    const uint32_t num_shader_engines       = asic_parameters.shader_engines;

    // NOTE: This array cannot be marked as const due to minor design flaw in GpaCounterContextHardwareInfo.
    std::array hardware_attrs = {GpaHardwareAttribute{kGpaHardwareAttributeNumShaderEngines, num_shader_engines},
                                 GpaHardwareAttribute{kGpaHardwareAttributeNumShaderArrays, num_sa},
                                 GpaHardwareAttribute{kGpaHardwareAttributeNumComputeUnits, num_cu},
                                 GpaHardwareAttribute{kGpaHardwareAttributeNumSimds, num_simd},
                                 GpaHardwareAttribute{kGpaHardwareAttributeNumWavesPerSimd, num_wave_fronts_per_simd}};

    // NOTE: The following attributes have been ignored since at least GPA v3.8
    // kGpaHardwareAttributeNumRenderBackends
    // kGpaHardwareAttributeClocksPerPrimitive
    // kGpaHardwareAttributeNumPrimitivePipes
    // kGpaHardwareAttributePeakVerticesPerClock
    // kGpaHardwareAttributePeakPrimitivesPerClock
    // kGpaHardwareAttributePeakPixelsPerClock

    counter_context_hardware_info.gpa_hardware_attribute_count = static_cast<uint32_t>(hardware_attrs.size());
    counter_context_hardware_info.gpa_hardware_attributes      = hardware_attrs.data();

    // NOTE: we are always using kGpaApiVulkan here. Since all officially-supported APIs are PAL-based, we need to
    // specify one of the PAL-based APIs. If we used the actual API, things would not be handled correctly for OpenCL,
    // which uses fewer counter block instances for per-SE blocks (it auto-sums them). Since the counter data is populated
    // by PAL, using Vulkan here makes sure that Vulkan, DX12, OpenCL and HIP are all handled correctly within GPA.
    // Also: we use Vulkan instead of DX12 because GPA doesn't support DX12 on Linux.
    // We may need to revisit this for non-PAL-based (i.e. GENERIC) APIs.
    GpaCounterContext result;
    GpaStatus         gpa_status = gpa_func_table->GpaCounterLibOpenCounterContext(kGpaApiVulkan,
                                                                           kGpaSessionSampleTypeDiscreteCounter,
                                                                           counter_context_hardware_info,
                                                                           kGpaOpenContextDefaultBit | kGpaOpenContextEnableHardwareCountersBit,
                                                                           &result);
    assert(kGpaStatusOk == gpa_status);
    if (kGpaStatusOk != gpa_status)
    {
        return nullptr;
    }

    return result;
}

static RgpFileWriterStatus AppendChunkDerivedSpmDbLegacy(const DerivedSpmDbArguments& args)
{
    const auto& stream = args.stream;

    int64_t file_size;
    if (!stream->GetSize(&file_size) || file_size <= 0)
    {
        return kRgpFileWriterStatusFileReadError;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(file_size));

    // Read in data to buffer
    int64_t bytes_read;
    if (!stream->Seek(0) || !stream->Read(file_size, reinterpret_cast<char*>(buffer.data()), &bytes_read) || bytes_read != file_size)
    {
        return kRgpFileWriterStatusFileReadError;
    }

    // Get pointer to beginning of file buffer
    const uint8_t* file_buffer = buffer.data();

    // Read in the AsicInfo chunk
    const uint8_t* asic_start = nullptr;

    RgpFileWriterStatus status = GetPointerToFirstChunkWithType(&asic_start, file_buffer, static_cast<size_t>(file_size), kRgpFileChunkTypeAsicInfo);
    if (kRgpFileWriterStatusOk != status)
    {
        return status;
    }
    RgpFileChunkAsicInfo asic_info = DeserializeChunkAsicInfo(asic_start);

    // Read in the SpmDb chunk
    const uint8_t* spm_db_start = nullptr;

    status = GetPointerToFirstChunkWithType(&spm_db_start, file_buffer, static_cast<size_t>(file_size), kRgpFileChunkTypeSpmDatabase);
    if (kRgpFileWriterStatusOk != status)
    {
        return status;
    }

    GpaCounterContext counter_ctx = OpenGPACounterContext(asic_info);

    std::optional spm_timing_raw_db = LoadRawSpmLegacy(spm_db_start);
    if (!spm_timing_raw_db.has_value())
    {
        return kRgpFileWriterStatusSpmDbError;
    }

    std::unique_ptr<spm_db::DerivedSpmDataBase> derived_spm_db = nullptr;
    spm_db::DerivedFromRawSpmBuilder            derived_spm_builder(
        spm_timing_raw_db->second, spm_timing_raw_db->first.sample_interval, counter_ctx, args.filter_bad_spm_data, args.progress_callback);
    for (const devtrace::DerivedSpmCounter& counter : args.counters)
    {
        if (counter.counter_formula.empty())
        {
            derived_spm_builder.AddGpaCounter(counter.counter_name, counter.display_name, counter.components);
        }
        else
        {
            derived_spm_builder.AddCustomFormulaCounter(counter.counter_name, counter.counter_formula);
        }
    }

    for (const devtrace::DerivedSpmGroup& group : args.groups)
    {
        derived_spm_builder.AddGroup(group.group_name, group.group_description, group.counters);
    }
    spm_db::Result build_result = derived_spm_builder.Build(derived_spm_db);
    if (build_result != spm_db::Result::Ok)
    {
        return kRgpFileWriterStatusSpmDbError;
    }

    // Serialize the new chunk data to bytes
    const std::vector<uint8_t>& serialized = SerializeChunkDerivedSpmDb(*derived_spm_db, spm_timing_raw_db->first);

    int64_t bytes_written;
    if (!stream->Seek(file_size) || !stream->Write(serialized.size(), reinterpret_cast<const char*>(serialized.data()), &bytes_written))
    {
        return kRgpFileWriterStatusFileWriteError;
    }

    return bytes_written != static_cast<int64_t>(serialized.size()) ? kRgpFileWriterStatusFileWriteError : kRgpFileWriterStatusOk;
}

/// @brief Load SPM timing data from the SPM Session chunk in an RDF chunk file.
/// @param rdf_file The RDF chunk file to load from.
/// @return A pair containing the sample interval and the sample timestamps if successful,
///         or std::nullopt if the chunk is not present in the file or not in the expected format.
static std::optional<SpmTiming> LoadSpmSessionRdf(rdfChunkFile* rdf_file)
{
    static constexpr const char* kSpmSessionChunkId = "SpmSession";
    // We could also read the number of counters, but that is the same as the number of counter data chunks in the RDF file.

    int64_t num_spm_session_chunks = 0;
    rdfChunkFileGetChunkCount(rdf_file, kSpmSessionChunkId, &num_spm_session_chunks);
    if (num_spm_session_chunks == 0)
    {
        return std::nullopt;
    }

    int64_t header_size = 0;
    if (rdfChunkFileGetChunkHeaderSize(rdf_file, kSpmSessionChunkId, 0, &header_size) != rdfResultOk || header_size != sizeof(SpmSessionHeader))
    {
        return std::nullopt;
    }

    SpmSessionHeader header{};
    if (rdfChunkFileReadChunkHeader(rdf_file, kSpmSessionChunkId, 0, &header) != rdfResultOk)
    {
        return std::nullopt;
    }

    std::vector<uint64_t> sample_timestamps = std::vector<uint64_t>(header.num_timestamps, 0);
    if (rdfChunkFileReadChunkData(rdf_file, kSpmSessionChunkId, 0, sample_timestamps.data()) != rdfResultOk)
    {
        return std::nullopt;
    }

    return SpmTiming{header.sampling_interval, std::move(sample_timestamps)};
}

static std::optional<spm_db::RawSpmDataBase> LoadRawSpmRdf(rdfChunkFile* rdf_file, uint32_t num_samples)
{
    static constexpr const char* kSpmDataChunkId = "SpmCounterData";

    spm_db::RawSpmDataBase result(num_samples);

    int64_t num_counters = 0;
    rdfChunkFileGetChunkCount(rdf_file, kSpmDataChunkId, &num_counters);

    for (int counter_idx = 0; counter_idx < static_cast<int>(num_counters); counter_idx++)
    {
        SpmCounterDataHeader header{};
        if (rdfChunkFileReadChunkHeader(rdf_file, kSpmDataChunkId, counter_idx, &header) != rdfResultOk)
        {
            return std::nullopt;
        }

        int64_t data_size = 0;
        rdfChunkFileGetChunkDataSize(rdf_file, kSpmDataChunkId, counter_idx, &data_size);
        if (data_size != static_cast<int64_t>(header.data_size * num_samples))
        {
            return std::nullopt;
        }

        switch (header.data_size)
        {
        case 2:
        {
            std::vector<uint16_t> samples(num_samples, 0);
            if (rdfChunkFileReadChunkData(rdf_file, kSpmDataChunkId, counter_idx, samples.data()) != rdfResultOk)
            {
                return std::nullopt;
            }
            result.AddCounter(static_cast<GpaHwBlock>(header.gpu_block), header.block_instance, header.event_index, std::move(samples));
        }
        break;
        case 4:
        {
            std::vector<uint32_t> samples(num_samples, 0);
            if (rdfChunkFileReadChunkData(rdf_file, kSpmDataChunkId, counter_idx, samples.data()) != rdfResultOk)
            {
                return std::nullopt;
            }
            result.AddCounter(static_cast<GpaHwBlock>(header.gpu_block), header.block_instance, header.event_index, std::move(samples));
        }
        break;
        default:
            return std::nullopt;
        }
    }

    return result;
}

RgpFileWriterStatus AppendChunkDerivedSpmDb(const DerivedSpmDbArguments& args)
{
    const auto& stream = args.stream;
    if (stream == nullptr || !stream->Open())
    {
        return kRgpFileWriterStatusFileReadError;
    }

    if (!args.is_rdf)
    {
        return AppendChunkDerivedSpmDbLegacy(args);
    }

    rdfUserStream       user_stream;
    rdfStream*          rdf_stream        = nullptr;
    rdfChunkFile*       chunk_file        = nullptr;
    rdfChunkFileWriter* chunk_file_writer = nullptr;

    auto cleanup = [&]() {
        rdfChunkFileWriterDestroy(&chunk_file_writer);
        rdfChunkFileClose(&chunk_file);
        rdfStreamClose(&rdf_stream);
    };

    stream->GetRdfUserStream(user_stream);

    if (rdfStreamCreateFromUserStream(&user_stream, &rdf_stream) != rdfResultOk)
    {
        cleanup();
        return kRgpFileWriterStatusFileReadError;
    }

    if (rdfChunkFileOpenStream(rdf_stream, &chunk_file) != rdfResultOk)
    {
        cleanup();
        return kRgpFileWriterStatusFileReadError;
    }

    RgpFileChunkAsicInfoRdfV3 asic_info;
    RgpFileWriterStatus       status = DeserializeChunkAsicInfo(chunk_file, asic_info);

    if (status != kRgpFileWriterStatusOk)
    {
        cleanup();
        return status;
    }

    auto spm_session = LoadSpmSessionRdf(chunk_file);
    if (!spm_session.has_value())
    {
        cleanup();
        return kRgpFileWriterStatusParserError;
    }

    std::optional<spm_db::RawSpmDataBase> raw_spm_db = LoadRawSpmRdf(chunk_file, static_cast<uint32_t>(spm_session->timestamps.size()));
    if (!raw_spm_db.has_value())
    {
        cleanup();
        return kRgpFileWriterStatusParserError;
    }

    GpaCounterContext ctx = OpenGPACounterContext(asic_info.v2);
    if (ctx == nullptr)
    {
        cleanup();
        return kRgpFileWriterStatusSpmDbError;
    }

    std::unique_ptr<spm_db::DerivedSpmDataBase> derived_spm_db = nullptr;
    spm_db::DerivedFromRawSpmBuilder derived_spm_builder(*raw_spm_db, spm_session->sample_interval, ctx, args.filter_bad_spm_data, args.progress_callback);

    for (const devtrace::DerivedSpmCounter& counter : args.counters)
    {
        if (counter.counter_formula.empty())
        {
            derived_spm_builder.AddGpaCounter(counter.counter_name, counter.display_name, counter.components);
        }
        else
        {
            derived_spm_builder.AddCustomFormulaCounter(counter.counter_name, counter.counter_formula);
        }
    }

    for (const devtrace::DerivedSpmGroup& group : args.groups)
    {
        derived_spm_builder.AddGroup(group.group_name, group.group_description, group.counters);
    }
    spm_db::Result build_result = derived_spm_builder.Build(derived_spm_db);
    if (build_result != spm_db::Result::Ok)
    {
        return kRgpFileWriterStatusSpmDbError;
    }

    rdfChunkFileWriterCreateInfo writer_create_info{};
    writer_create_info.stream       = rdf_stream;
    writer_create_info.appendToFile = true;

    if (rdfChunkFileWriterCreate2(&writer_create_info, &chunk_file_writer) == rdfResultOk)
    {
        AppendChunks(*derived_spm_db, chunk_file_writer);
    }

    cleanup();
    return kRgpFileWriterStatusOk;
}

static bool AppendChunk(rdfChunkFileWriter* chunk_file_writer,
                        const std::string&  chunk_id,
                        size_t              header_size,
                        const void*         header,
                        size_t              chunk_size,
                        const void*         data)
{
    rdfChunkCreateInfo create_info{};
    create_info.version    = 1;
    create_info.headerSize = header_size;
    create_info.pHeader    = header;

    memcpy(create_info.identifier, chunk_id.c_str(), chunk_id.size());

    int index = 0;
    return rdfChunkFileWriterWriteChunk(chunk_file_writer, &create_info, chunk_size, data, &index) == rdfResultOk;
}

struct DerivedSpmCounterToAppend
{
    DerivedSpmCounterHeader header{};
    const double*           values               = nullptr;
    const uint32_t*         component_index_list = nullptr;

    std::string counter_name;
    std::string counter_description;
};

static bool AppendCounter(const DerivedSpmCounterToAppend& counter_to_append, uint32_t number_of_timestamps, rdfChunkFileWriter* chunk_file_writer)
{
    std::vector<uint8_t> data(sizeof(double) * number_of_timestamps);
    memcpy(data.data(), counter_to_append.values, data.size());

    size_t       head           = data.size();
    const size_t component_size = sizeof(uint32_t) * counter_to_append.header.component_count;

    if (counter_to_append.component_index_list != nullptr)
    {
        data.resize(head + component_size);
        memcpy(data.data() + head, counter_to_append.component_index_list, component_size);
    }

    head = data.size();
    data.resize(head + counter_to_append.header.name_length);
    memcpy(data.data() + head, counter_to_append.counter_name.c_str(), counter_to_append.header.name_length);

    head = data.size();
    data.resize(head + counter_to_append.header.desc_length);
    memcpy(data.data() + head, counter_to_append.counter_description.c_str(), counter_to_append.header.desc_length);

    return AppendChunk(chunk_file_writer, kDerivedSpmCounterChunkId, sizeof(counter_to_append.header), &counter_to_append.header, data.size(), data.data());
}

static bool AppendCounter(uint32_t                                                              pci_id,
                          const spm_db::DerivedSpmCounter&                                      counter,
                          const std::unordered_map<const spm_db::DerivedSpmCounter*, uint32_t>& counter_index_map,
                          uint32_t                                                              counter_index,
                          uint32_t                                                              number_of_timestamps,
                          rdfChunkFileWriter*                                                   chunk_file_writer)
{
    DerivedSpmCounterToAppend counter_to_append{};
    counter_to_append.header.pci_id          = pci_id;
    counter_to_append.header.counter_index   = counter_index;
    counter_to_append.header.component_count = static_cast<uint32_t>(counter.Components().size());
    counter_to_append.header.unit            = static_cast<CounterUsageType>(counter.UsageType());
    counter_to_append.header.name_length     = static_cast<uint32_t>(counter.DisplayName().length());
    counter_to_append.header.desc_length     = static_cast<uint32_t>(counter.Description().length());

    counter_to_append.values = counter.Samples().data();

    std::vector<uint32_t> component_index_list;
    for (const auto& component : counter.Components())
    {
        component_index_list.push_back(counter_index_map.at(component.counter));
    }
    counter_to_append.component_index_list = component_index_list.data();

    counter_to_append.counter_name        = counter.DisplayName().c_str();
    counter_to_append.counter_description = counter.Description().c_str();

    return AppendCounter(counter_to_append, number_of_timestamps, chunk_file_writer);
}

static bool AppendGroup(uint32_t                                                              pci_id,
                        uint32_t                                                              group_index,
                        const spm_db::DerivedSpmGroup&                                        group_info,
                        const std::unordered_map<const spm_db::DerivedSpmCounter*, uint32_t>& counter_index_map,
                        rdfChunkFileWriter*                                                   chunk_file_writer)
{
    DerivedSpmGroupHeader header{};
    header.pci_id       = pci_id;
    header.group_index  = group_index;
    header.member_count = static_cast<uint32_t>(group_info.Members().size());
    header.name_length  = static_cast<uint32_t>(group_info.Name().length());
    header.desc_length  = static_cast<uint32_t>(group_info.Description().length());

    std::vector<uint8_t> data(sizeof(uint32_t) * header.member_count);
    uint32_t*            counter_index_list = reinterpret_cast<uint32_t*>(data.data());

    for (uint32_t member_index = 0; member_index < header.member_count; ++member_index)
    {
        counter_index_list[member_index] = counter_index_map.at(group_info.Members().at(member_index));
    }

    size_t head = data.size();
    data.resize(head + header.name_length);
    memcpy(data.data() + head, group_info.Name().c_str(), header.name_length);

    head = data.size();
    data.resize(head + header.desc_length);
    memcpy(data.data() + head, group_info.Description().c_str(), header.desc_length);

    return AppendChunk(chunk_file_writer, kDerivedSmpGroupChunkId, sizeof(header), &header, data.size(), data.data());
}

RgpFileWriterStatus AppendChunks(const spm_db::DerivedSpmDataBase& derived_spm_db, rdfChunkFileWriter* chunk_file_writer)
{
    // TODO: We're going to need to do some more logic with the PCI id right now we're just going to use 0
    uint32_t pci_id = 0;

    DerivedSpmSession session{};
    session.pci_id               = pci_id;
    session.num_derived_counters = static_cast<uint32_t>(derived_spm_db.Counters().size());
    session.group_count          = static_cast<uint32_t>(derived_spm_db.Groups().size());

    if (!AppendChunk(chunk_file_writer, kDerivedSpmSessionChunkId, 0, nullptr, sizeof(session), &session))
    {
        return kRgpFileWriterStatusFileWriteError;
    }

    // Iterate through the derived SPM DB's counters to generate an arbitrary ordering for serialization.

    // A mapping from a counter's canonical name to its index in the serialization order.
    std::unordered_map<const spm_db::DerivedSpmCounter*, uint32_t> counter_index_map;

    uint32_t counter_index = 0;
    for (const auto& counter_info : derived_spm_db.Counters())
    {
        counter_index_map.insert({&counter_info.second, counter_index});
        counter_index++;
    }

    counter_index = 0;
    for (const auto& counter_info : derived_spm_db.Counters())
    {
        if (!AppendCounter(pci_id, counter_info.second, counter_index_map, counter_index++, derived_spm_db.NumSamples(), chunk_file_writer))
        {
            return kRgpFileWriterStatusFileWriteError;
        }
    }

    for (uint32_t group_index = 0; group_index < derived_spm_db.Groups().size(); ++group_index)
    {
        if (!AppendGroup(pci_id, group_index, derived_spm_db.Groups().at(group_index), counter_index_map, chunk_file_writer))
        {
            return kRgpFileWriterStatusFileWriteError;
        }
    }

    return kRgpFileWriterStatusOk;
}
