// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition of the RGP file format.
///
///        This is copied from the RGP backend.

#ifndef RDP_SOURCE_TRACE_INC_RGP_FILE_FORMAT_H_
#define RDP_SOURCE_TRACE_INC_RGP_FILE_FORMAT_H_

#include <stdint.h>

/// Magic number for all RGP files.
static const int kRgpFileMagicNumber = 0x50303042;

/// The maximum size of the GPU name in <c><i>RgpFileChunkAsicInfo</i></c>.
static const int kRgpMaxGpuNameInFile = 256;

/// The active CU mask is 1024 bits long
static const int kRgpActiveCuMaskLenInFileBytes = 128;

/// The active pixel packer mask is 32 bits long
static const int kRgpActivePixelPackerMaskLenInFileBytes = 4;

/// @brief Structure encapsulating the file header of a RGP file.
typedef struct RgpFileHeader
{
    uint32_t magic_number;         ///< Magic number, always set to <c><i>kRgpFileMagicNumber</i></c>.
    uint32_t version_major;        ///< The major version number of the file.
    uint32_t version_minor;        ///< The minor version number of the file.
    uint32_t flags;                ///< Bitfield of flags set with information about the file.
    int32_t  chunk_offset;         ///< The offset in bytes to the first chunk contained in the file.
    int32_t  second;               ///< The second in the minute that the RGP file was created.
    int32_t  minute;               ///< The minute in the hour that the RGP file was created.
    int32_t  hour;                 ///< The hour in the day that the RGP file was created.
    int32_t  day_in_month;         ///< The day in the month that the RGP file was created.
    int32_t  month;                ///< The month in the year that the RGP file was created.
    int32_t  year;                 ///< The year that the RGP file was created.
    int32_t  day_in_week;          ///< The day in the week that the RGP file was created.
    int32_t  day_in_year;          ///< The day in the year that the RGP file was created.
    int32_t  is_daylight_savings;  ///< Set to 1 if the time is subject to daylight savings.
} RgpFileHeader;

/// @brief An enumeration of all chunk types used in the file format.
typedef enum RgpFileChunkType : uint8_t
{
    kRgpFileChunkTypeAsicInfo              = 0,   ///< A chunk containing the description of the ASIC on which the profile was captured.
    kRgpFileChunkTypeSqttDesc              = 1,   ///< A chunk containing the description of the SQTT data.
    kRgpFileChunkTypeSqttData              = 2,   ///< A chunk containing the SQTT data for a single shader engine.
    kRgpFileChunkTypeApiInfo               = 3,   ///< A chunk containing the description of the API on which the profile was captured.
    kRgpFileChunkTypeIsaDatabase           = 4,   ///< A chunk containing a shader database. Driver support dropped (superceded by code object database).
    kRgpFileChunkTypeQueueTimings          = 5,   ///< A chunk containing command buffer, singla, wait and semaphore timings.
    kRgpFileChunkTypeClockCalibration      = 6,   ///< A chunk containing clock calibration data, enough to map between different clock domains.
    kRgpFileChunkTypeCpuInfo               = 7,   ///< A chunk containing the description of the CPU on which the profile was captured.
    kRgpFileChunkTypeSpmDatabase           = 8,   ///< A chunk containing SPM information.
    kRgpFileChunkTypeCodeObjectDatabase    = 9,   ///< A chunk containing code object database.
    kRgpFileChunkTypeApiLevelLoaderEvents  = 10,  ///< A chunk containing loader events.
    kRgpFileChunkTypePsoCorrelation        = 11,  ///< A chunk containing PSO correlation information.
    kRgpFileChunkTypeAltDerivedSpmDatabase = 12,  ///< A chunk containing derived SPM information (legacy ordinal value).
    kRgpFileChunkTypeDataFabricSpmDatabase = 13,  ///< A chunk containing data fabric SPM information.
    kRgpFileChunkTypeInstrumentationTable  = 14,  ///< A chunk containing instrumentation tables.

    // NOTE: Add new chunks above this.
    kRgpFileChunkTypeCount,  ///< The number of different chunk types.

    // Tools-defined chunk types.
    kRgpFileChunkTypeFirstToolsType     = 128,                                  ///< The first index of tools-defined chunks.
    kRgpFileChunkTypeDerivedSpmDatabase = kRgpFileChunkTypeFirstToolsType,      ///< A chunk containing derived SPM information.
    kRgpFileChunkTypeDriverSettings     = kRgpFileChunkTypeFirstToolsType + 1,  ///< A chunk that contains the driver settings overrides.

    // NOTE: Add new tools-defined chunks above this.
    kRgpFileChunkTypeLastToolsType  ///< The last index of tools-defined chunks.
} RgpFileChunkType;

static_assert(kRgpFileChunkTypeCount <= kRgpFileChunkTypeFirstToolsType, "Overlap between driver-defined chunks and tools-defined chunks");

/// @brief A structure encapsulating a single chunk identifier.
///
/// A chunk identifier comprises of the chunk type, and an index. The index is unique for each instance of the chunk.
/// For example, if a specific ASIC had 4 Shader Engines there would be multiple RGP_FILE_CHUNK_SQTT_DATA with
/// indicies ranging from [0..3].
typedef struct RgpFileChunkIdentifier
{
    union
    {
        struct
        {
            RgpFileChunkType chunk_type : 8;   ///< The type of chunk.
            int8_t           chunk_index : 8;  ///< The index of the chunk.
            int16_t          reserved : 16;    ///< Reserved, set to 0.
        };

        uint32_t value;  ///< 32bit value containing all the above fields.
    };
} RgpFileChunkIdentifier;

/// Major version of the Spm Db chunk when the size members were added.
static const int kRgpFileChunkTypeSpmDbSizeMembersAddedMajorVersion = 2;

/// @brief A structure encapsulating common fields of a chunk in the RGP file format.
///
/// Note the ordering of minor and major version fields is important for backward compatibility with older RGP files
/// where the chunk header versions were encoded as single 32 bit numbers.
/// This way version changes on older files will load as minor versions.
typedef struct RgpFileChunkHeader
{
    RgpFileChunkIdentifier chunk_identifier;  ///< A unique identifier for the chunk.
    int16_t                version_minor;     ///< The minor version of the chunk. Please see above note on ordering of minor and major version.
    int16_t                version_major;     ///< The major version of the chunk.
    int32_t                size_in_bytes;     ///< The size of the chunk in bytes.
    int32_t                padding;           ///< Reserved padding dword.
} RgpFileChunkHeader;

/// @brief A structure encapsulating version 1 of the Streaming Performance Counter/ Metric (SPM) data.
typedef struct RgpFileChunkSpmDbV1
{
    RgpFileChunkHeader header;                      ///< Common header for all chunks.
    uint32_t           flags;                       ///< Chunk specific flags reserved for future use.
    uint32_t           number_of_timestamps;        ///< Number of timestamps in the SPM data.
    uint32_t           number_of_spm_counter_info;  ///< Number of SpmCounterInfo structs in this chunk.
    uint32_t           sampling_interval;           ///< Sampling interval used when collecting SPM data.
    // NOTE The following data follows:
    // uint64_t Timestamps[number_of_timestamps]
    // SpmCounterInfo[number_of_spm_counter_info]
    // CounterData[number_of_spm_counter_info * number_of_timestamps]
} RgpFileChunkSpmDbV1;

/// @brief A structure encapsulating version 2 (and newer) of the Streaming Performance Counter/ Metric (SPM) data.
typedef struct RgpFileChunkSpmDb
{
    RgpFileChunkHeader header;                      ///< Common header for all chunks.
    uint32_t           flags;                       ///< Chunk specific flags reserved for future use.
    uint32_t           preamble_size;               ///< Size in bytes of the fixed parts of this struct (gives the offset of the timestamp data).
    uint32_t           number_of_timestamps;        ///< Number of timestamps in the SPM data.
    uint32_t           number_of_spm_counter_info;  ///< Number of SpmCounterInfo structs in this chunk.
    uint32_t           spm_counter_info_size;       ///< The size in bytes of a single SpmCounterInfo item.
    uint32_t           sampling_interval;           ///< Sampling interval used when collecting SPM data.
    // NOTE The following data follows:
    // uint64_t Timestamps[number_of_timestamps]
    // SpmCounterInfo[number_of_spm_counter_info]
    // CounterData[number_of_spm_counter_info * number_of_timestamps]
} RgpFileChunkSpmDb;

/// @brief A structure encapsulating information about the ASIC on which the profile was captured.
///
/// NOTE: Please be careful when adding things to this struct to account for padding
///       introduced for new members. For example, the alignment of this structure is
///       currently 8 bytes, due to numerous 64bit fields. This means that when adding
///       new 64bit fields, care should be taken not to introduce unused padding bytes
///       into the structure. To this end the code is formatted such that it easy to
///       see where potential padding may be added.
typedef struct RgpFileChunkAsicInfo
{
    // clang-format off
    RgpFileChunkHeader              header;                       ///< Common header for all chunks.

    uint64_t                        flags;                        ///< Flags for the ASIC info chunk.

    int64_t                         shader_core_clock;            ///< The shader core clock frequency.

    int64_t                         memory_clock;                 ///< The memory clock frequency.

    int32_t                         device_id;                    ///< The device ID for the card where the profile was captured.
    int32_t                         device_revision_id;           ///< The device revision ID for the card where the profile was captured.

    int32_t                         vgprs_per_simd;               ///< The number of VGPRs per SIMD.
    int32_t                         sgprs_per_simd;               ///< The number of SGPRs per SIMD.

    int32_t                         shader_engines;                  ///< The number of shader engines.
    int32_t                         compute_unit_per_shader_engine;  ///< The number of compute units per shader engine.

    int32_t                         simd_per_compute_unit;        ///< The number of SIMDs per compute unit.
    int32_t                         wavefronts_per_simd;          ///< The number of wavefronts per SIMD.

    int32_t                         minimum_vgpr_alloc;           ///< Minimum number of VGPRs per wavefront.
    int32_t                         vgpr_alloc_granularity;       ///< The allocation granularity of VGPRs.

    int32_t                         minimum_sgpr_alloc;           ///< Minimum number of SGPRs per wavefront.
    int32_t                         sgpr_alloc_granularity;       ///< The allocation granularity of SGPRs.

    int32_t                         hardware_contexts;            ///< The number of hardware contexts.
    int32_t                         gpu_type;                     ///< The GPU type.

    int32_t                         gfx_ip_level;                 ///< The graphics IP level of the GPU.
    int32_t                         gpu_index;                    ///< The index of the GPU in an MGPU rig.

    int32_t                         gds_size;                     ///< The total size of GDS available to the GPU.
    int32_t                         gds_per_shader_engine_size;   ///< The total size of GDS available to each shader engine.

    int32_t                         ce_ram_size;                  ///< The total size of CE RAM.
    int32_t                         ce_ram_size_graphics;         ///< The max CE RAM size available to graphics engine in bytes.

    int32_t                         ce_ram_size_compute;          ///< The max CE RAM size available to Compute engine in bytes.
    int32_t                         maximum_dedicated_cu;         ///< The total number of CUs dedicated to real-time audio queue.

    uint64_t                        vram_size;                    ///< The total size of VRAM available to the GPU.

    int32_t                         vram_bus_width;               ///< The width of the memory bus to VRAM.
    int32_t                         level2_cache_size;            ///< The total size of the L2 cache (TCC).

    int32_t                         level1_cache_size;            ///< The size of the L1 cache (TCP) for each compute unit. This is L0 on RDNA hardware
    int32_t                         lds_size;                     ///< The size of LDS for each compute unit.

    char                            gpu_name[kRgpMaxGpuNameInFile];  ///< The name of the GPU.

    float                           alu_per_clock;                ///< The number of ALUs per clock.
    float                           textures_per_clock;           ///< The number of texture per clock.

    float                           primitives_per_clock;         ///< The number of primitives per clock.
    float                           pixels_per_clock;             ///< The number of pixels per clock.

    uint64_t                        gpu_timestamp_frequency;      ///< The frequency (in Hz) of the clock used for generating timestamps on GPU.

    uint64_t                        max_shader_core_clock;        ///< The max (peak) shader core clock frequency.
    uint64_t                        max_memory_clock;             ///< The max (peak) memory clock frequency.

    // Chunk version 0.1
    uint32_t                        memory_ops_per_clock;         ///< The number of video memory operations per clock cycle.
    uint32_t                        memory_chip_type;             ///< The type of video memory chip (DDR, GDDR, HBM, ...). See RgpVideoMemoryChipType enum.

    // Chunk version 0.2
    uint32_t                        lds_allocation_granularity;   ///< The LDS allocation granularity expressed in bytes.

    // Chunk version 0.3
    char                            active_cu_mask[kRgpActiveCuMaskLenInFileBytes];            ///< The active CU mask.
    char                            active_cu_mask_reserved[kRgpActiveCuMaskLenInFileBytes];   ///< Reserved memory for active CU mask.

    // Chunk version 0.5
    char                            active_pixel_packer_mask[kRgpActivePixelPackerMaskLenInFileBytes];            ///< The active Pixel Packer mask.
    char                            active_pixel_packer_mask_reserved[kRgpActivePixelPackerMaskLenInFileBytes];   ///< Reserved memory for active Pixel Packer mask.

    int32_t                         gl1_cache_size;          ///< The total size of the GL1 cache (per shader array)
    int32_t                         instruction_cache_size;  ///< The total size of the Instruction cache per compute unit.
    int32_t                         scalar_cache_size;       ///< The total size of the Scalar cache (K$) per compute unit.
    int32_t                         mall_cache_size;         ///<The total size of the MALL (Infinity) cache.

    // clang-format on
} RgpFileChunkAsicInfo;

static constexpr size_t kTraceGpuNameMaxSize      = 256;  ///< The maximum length of a GPU name.
static constexpr size_t kTraceGpuMaxShaderEngines = 32;   ///< The maximum number of SE per GPU.
static constexpr size_t kMaxShaderArraysPerEngine = 2;    ///< Maximum number of SA per SE.

/// @brief Class of GPU of an ASIC.
enum class TraceGpuType : uint32_t
{
    Unknown,
    Integrated,
    Discrete,
    Virtual
};

/// @brief Graphics IP level of an ASIC.
struct TraceGfxIpLevel
{
    uint16_t major;
    uint16_t minor;
    uint16_t stepping;
};

/// @brief Type of memory chip used by an ASIC.
enum class TraceMemoryType : uint32_t
{
    Unknown,
    Ddr,
    Ddr2,
    Ddr3,
    Ddr4,
    Ddr5,
    Gddr3,
    Gddr4,
    Gddr5,
    Gddr6,
    Hbm,
    Hbm2,
    Hbm3,
    Lpddr4,
    Lpddr5
};

/// @brief Version 2 of the ASIC info found in RDF files.
struct RgpFileChunkAsicInfoRdfV2
{
    uint32_t        pci_id;                          ///< The ID of the GPU queried
    uint64_t        shader_clock_frequency;          ///< Gpu core clock frequency in Hz
    uint64_t        memory_clock_frequency;          ///< Memory clock frequency in Hz
    uint64_t        gpu_timestamp_frequency;         ///< Frequency of the gpu timestamp clock in Hz
    uint64_t        max_shader_core_clock;           ///< Maximum shader core clock frequency in Hz
    uint64_t        max_memory_clock;                ///< Maximum memory clock frequency in Hz
    int32_t         device_id;                       ///<PCIE device id
    int32_t         device_revision_id;              ///< PCIE revision id
    int32_t         vgprs_per_simd;                  ///< Number of VGPRs per SIMD
    int32_t         sgprs_per_simd;                  ///< Number of SGPRs per SIMD
    int32_t         shader_engines;                  ///< Number of shader engines
    int32_t         compute_unit_per_shader_engine;  ///< Number of compute units per shader engine
    int32_t         simd_per_compute_unit;           ///< Number of SIMDs per compute unit
    int32_t         wavefronts_per_simd;             ///< Number of wavefronts per SIMD
    int32_t         minimum_vgpr_alloc;              ///< Minimum number of VGPRs per wavefronttring
    int32_t         vgpr_alloc_granularity;          ///< Allocation granularity of VGPRs
    int32_t         minimum_sgpr_alloc;              ///< Minimum number of SGPRs per wavefront
    int32_t         sgpr_alloc_granularity;          ///< Allocation granularity of SGPRs
    int32_t         hardware_contexts;               ///< Number of hardware contexts
    TraceGpuType    gpu_type;                        ///< The class of GPU (Discrete, Virtual, etc.)
    TraceGfxIpLevel gfx_ip_level;                    ///< The GFX IP level of the GPU
    int32_t         gpu_index;                       ///< The PAL index of the GPU
    int32_t         ce_ram_size;                     ///< Max size in bytes of CE RAM space available
    int32_t         ce_ram_size_graphics;            ///< Max CE RAM size available to graphics engine in bytes
    int32_t         ce_ram_size_compute;             ///< Max CE RAM size available to Compute engine in bytes
    int32_t         max_number_of_dedicated_cus;     ///< Number of CUs dedicated to real time audio queue
    int64_t         vram_size;                       ///< Total number of bytes to VRAM
    int32_t         vram_bus_width;                  ///< Width of the bus to VRAM
    int32_t         l_2_cache_size;                  ///< Total number of bytes in L2 Cache (TCC on GCN hardware, GL2C on RDNA hardware)
    int32_t         l_1_cache_size;                  ///< Total number of L1 cache bytes per CU (TCP); this is L0 on RDNA hardware
    int32_t         lds_size;                        ///< Total number of LDS bytes per CU
    char            gpu_name[kTraceGpuNameMaxSize];  ///< Name of the GPU, padded to 256 bytes
    float           alu_per_clock;                   ///< Number of ALUs per clock
    float           texture_per_clock;               ///< Number of texture per clock
    float           prims_per_clock;                 ///< Number of primitives per clock
    float           pixels_per_clock;                ///< Number of pixels per clock
    uint32_t        memory_ops_per_clock;            ///< Number of memory operations per memory clock cycle
    TraceMemoryType memory_chip_type;
    uint32_t        lds_granularity;                                                ///< LDS allocation granularity expressed in bytes
    uint16_t        cu_mask[kTraceGpuMaxShaderEngines][kMaxShaderArraysPerEngine];  ///< Mask of present, non-harvested CUs (physical layout)
};

/// @brief Version 3 of the ASIC info found in RDF files.
struct RgpFileChunkAsicInfoRdfV3
{
    RgpFileChunkAsicInfoRdfV2 v2;   ///< Version 2 of the chunk.
    uint32_t pixel_packer_mask[4];  ///< Mask of present, non-harvested pixel packers -- 4 bits per shader engine (up to a max of 32 shader engines)
    uint32_t gl_1_cache_size;       ///< Total number of GL1 cache bytes per shader array
    uint32_t inst_cache_size;       ///< Total number of Instruction cache bytes per CU
    uint32_t scalar_cache_size;     ///< Total number of Scalar cache (K$) bytes per CU
    uint32_t mall_cache_size;       ///< Total number of MALL cache (Infinity cache) bytes
};

/// @brief Header for SPM session chunk in RDF files.
struct SpmSessionHeader
{
    uint32_t pci_id;             ///< The ID of the GPU the trace ran on
    uint32_t flags;              ///< SPM trace configuration flags (reserved for future use)
    uint32_t sampling_interval;  ///< Perf. counter sampling interval
    uint32_t num_timestamps;     ///< Number of timestamps in the SPM trace data
    uint32_t num_spm_counters;   ///< Number of SPM counters sampled
};

/// @brief Specifies a particular block on the GPU to gather counters for. Matches "Pal::GpuBlock" in "palPerfExperiment.h"
enum class SpmGpuBlock : uint32_t
{
    kCpf = 0x0,
    kIa  = 0x1,
    /* ...snip... */
    kCount
};

/// @brief Header for SPM data header chunk.
struct SpmCounterDataHeader
{
    uint32_t    pci_id;          ///< The ID of the GPU the trace ran on
    SpmGpuBlock gpu_block;       ///< GPU block encoding
    uint32_t    block_instance;  ///< Instance of the block in the ASIC
    uint32_t    event_index;     ///< Index of the perf. counter event within the block
    uint32_t    data_size;       ///< Size (in bytes) of a single counter data item
};

#endif
