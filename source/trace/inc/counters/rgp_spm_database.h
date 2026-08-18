// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition of SPM database and associated structures.

#ifndef RDP_SOURCE_TRACE_SRC_COUNTERS_RGP_SPM_DATABASE_H_
#define RDP_SOURCE_TRACE_SRC_COUNTERS_RGP_SPM_DATABASE_H_

#include <stdint.h>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <vector>

#include <amdrdf.h>

#include <logging.h>

#include <spm_db/gpa_counters_loader.h>

#include <rgp_file_format.h>

/// Typedef for error codes returned from functions in the RGP backend.
typedef int32_t RgpErrorCode;

static const RgpErrorCode kRgpOk                              = 0;           ///< The operation completed successfully.
static const RgpErrorCode kRgpErrorInvalidPointer             = 0x80000000;  ///< The operation failed due to an invalid pointer.
static const RgpErrorCode kRgpErrorInvalidAlignment           = 0x80000001;  ///< The operation failed due to an invalid pointer.
static const RgpErrorCode kRgpErrorInvalidSize                = 0x80000002;  ///< The operation failed due to an invalid size.
static const RgpErrorCode kRgpEof                             = 0x80000003;  ///< The end of the file was encountered.
static const RgpErrorCode kRgpErrorInvalidPath                = 0x80000004;  ///< The operation failed because the specified path was invalid.
static const RgpErrorCode kRgpEndOfFile                       = 0x80000005;  ///< The operation failed because end of file was reached.
static const RgpErrorCode kRgpErrorMalformedData              = 0x80000006;  ///< The operation failed because of some malformed data.
static const RgpErrorCode kRgpErrorIndexOutOfRange            = 0x80000007;  ///< The operation failed because an index was out of range.
static const RgpErrorCode kRgpErrorFileNotOpen                = 0x80000008;  ///< The operation failed because a file was not open.
static const RgpErrorCode kRgpErrorOutOfMemory                = 0x80000009;  ///< The operation failed because it ran out memory.
static const RgpErrorCode kRgpErrorPlatformFunctionFailed     = 0x8000000a;  ///< The operation failed because of a platform-specific function.
static const RgpErrorCode kRgpErrorUnsupported                = 0x8000000b;  ///< The operation failed because it was unsupported.
static const RgpErrorCode kRgpErrorContextSwitchRequired      = 0x8000000c;  ///< The operation requires a context switch to occur.
static const RgpErrorCode kRgpErrorKillWavefront              = 0x8000000d;  ///< The operation requires a wavefront to be killed.
static const RgpErrorCode kRgpErrorUnsupportedLowSpecVersion  = 0x8000000e;  ///< The operation failed because a specification version was too low.
static const RgpErrorCode kRgpErrorUnsupportedHighSpecVersion = 0x8000000f;  ///< The operation failed because a specification version was too high.
static const RgpErrorCode kRgpErrorComgrFailure               = 0x80000010;  ///< The operation failed because of an error in comgr.
static const RgpErrorCode kRgpErrorEventCountExceeded         = 0x80000011;  ///< The operation failed because the event count has been exceeded.
static const RgpErrorCode kRgpErrorDataNotFound               = 0x80000012;  ///< The operation failed because the data requested could not be found.
static const RgpErrorCode kRgpErrorAmdGpuDisFailure           = 0x80000013;  ///< The operation failed because of an error in amdgpu-dis.
static const RgpErrorCode kRgpErrorGfx10NotSynchronized       = 0x80000014;  ///< The operation failed because shader engine clocks could not be synchronized.
static const RgpErrorCode kRgpErrorGpaFailure                 = 0x80000015;  ///< The operation failed because of an error in GPA library.
static const RgpErrorCode kRgpErrorShaderNotFound             = 0x80000016;  ///< The operation failed because a shader cannot be found.
static const RgpErrorCode kRgpErrorShaderIsaNotInitialized    = 0x80000017;  ///< The operation failed because the shader ISA has not been initialized.
static const RgpErrorCode kRgpErrorAborted                    = 0xffffffff;  ///< The operation failed because it was aborted.

/// @brief An enumeration of GFX IP levels.
typedef enum RgpGfxIpLevel
{
    kRgpGfxLevelNone = 0,   ///< Not a GCN GPU.
    kRgpGfxLevel6    = 1,   ///< Graphics IP level 6    (Tahiti, Pitcairn etc.).
    kRgpGfxLevel7    = 2,   ///< Graphics IP level 7    (Hawaii, Kaveri, etc.).
    kRgpGfxLevel8    = 3,   ///< Graphics IP level 8    (Tonga, Carrizo, etc.).
    kRgpGfxLevel8_1  = 4,   ///< Graphics IP level 8.1  (Stoney, etc.).
    kRgpGfxLevel9    = 5,   ///< Graphics IP level 9    (Vega10, etc.).
    kRgpGfxLevel10   = 6,   ///< Graphics IP level 10   (internal).
    kRgpGfxLevel10_1 = 7,   ///< Graphics IP level 10.1 (Navi10, etc.).
    kRgpGfxLevel10_2 = 8,   ///< Graphics IP level 10.2 (internal).
    kRgpGfxLevel10_3 = 9,   ///< Graphics IP level 10.3 (Navi21, etc.).
    kRgpGfxLevel11   = 10,  ///< Graphics IP level 11   (Navi31, etc.).

    // convenience values based on the above real values.
    kRgpGfxLevelMinSupported = kRgpGfxLevel8,     ///< Minimum GFX IP level supported.
    kRgpGfxLevelMaxSupported = kRgpGfxLevel10_3,  ///< Maximum GFX IP level supported. (TODO: change this value once we have a valid GFX11 profile.)
    kRgpGfxLevelMinGfx8      = kRgpGfxLevel8,     ///< Minimum GFX8 GFX IP level supported.
    kRgpGfxLevelMaxGfx8      = kRgpGfxLevel8_1,   ///< Maximum GFX8 GFX IP level supported.
    kRgpGfxLevelMinGfx10     = kRgpGfxLevel10,    ///< Minimum GFX10 GFX IP level supported.
    kRgpGfxLevelMaxGfx10     = kRgpGfxLevel10_3,  ///< Maximum GFX10 GFX IP level supported.
    kRgpGfxLevelMinGfx11     = kRgpGfxLevel11,    ///< Minimum GFX11 GFX IP level supported.
    kRgpGfxLevelMaxGfx11     = kRgpGfxLevel11,    ///< Maximum GFX11 GFX IP level supported.

} RgpGfxIpLevel;

/// @brief A structure encapsulating Streaming Performance Monitor (SPM) counter info.
typedef struct SpmCounterInfo
{
    uint32_t gpu_block_id;        ///< GPU block identifier.
    uint32_t gpu_block_instance;  ///< GPU block instance.
    uint32_t data_offset;         ///< Offset from the start of counter_data for the data for this counter instance.
} SpmCounterInfo;

/// @brief A structure encapsulating the 1.2 version of Streaming Performance Monitor (SPM) counter info.
typedef struct SpmCounterInfo_V1_2 : public SpmCounterInfo
{
    // Chunk version 1.2.
    uint32_t event_index;  ///< Index of the perf counter event.
} SpmCounterInfo_V1_2;

/// @brief A structure encapsulating the 2.0 version of Streaming Performance Monitor (SPM) counter info.
typedef struct SpmCounterInfo_V2
{
    uint32_t gpu_block_id;        ///< GPU block identifier.
    uint32_t gpu_block_instance;  ///< GPU block instance.
    uint32_t event_index;         ///< Index of the perf counter event.
    uint32_t data_offset;         ///< Offset from the start of counter_data for the data for this counter instance, in bytes
    uint32_t data_size;           ///< Size in bytes of a single counter data item.
} SpmCounterInfo_V2;

/// @brief A structure encapsulating Streaming Performance Monitor (SPM) data as encoded in the RGP file chunk.
typedef struct RawRgpSpmDataBase
{
    uint32_t  flags;                       ///< Chunk specific flags reserved for future use.
    uint32_t  number_of_timestamps;        ///< Number of timestamps in the profile data.
    uint32_t  number_of_spm_counter_info;  ///< Number of SpmCounterInfo structs in this chunk.
    uint32_t  sampling_interval;           ///< Sampling interval specified during capture.
    uint64_t* timestamps;                  ///< Array of number_of_timestamps number of timestamps.
    void*     spm_counter_info;            ///< Array of number_of_spm_counter_info number of SpmCounterInfo.
    uint16_t* counter_data;                ///< Array of number_of_spm_counter_info * number_of_timestamps counter delta values.

    std::vector<GpaUInt32> counter_offsets;  ///< Used for the RDF flow, the offsets of each counter in the packed data in 16-bit words.

} RawRgpSpmDataBase;

/// @brief An enum used to specify the type of data represented by a counter.
///
/// This corresponds to the GPA_Usage_Type enum in GPA.
typedef enum CounterUsageType
{
    kCounterUsageTypeRatio,         ///< Counter value is expressed as a ratio of two different values or types.
    kCounterUsageTypePercentage,    ///< Counter value is expressed as a percentage, typically within [0,100] range, but may be higher for certain counters.
    kCounterUsageTypeCycles,        ///< Counter value is expressed in clock cycles.
    kCounterUsageTypeMilliseconds,  ///< Counter value is expressed in milliseconds.
    kCounterUsageTypeBytes,         ///< Counter value is expressed in bytes.
    kCounterUsageTypeItems,         ///< Counter value is expressed as a count of items or objects (ie, vertices, triangles, threads, pixels, texels, etc).
    kCounterUsageTypeKilobytes,     ///< Counter value is expressed in kilobytes.
    kCounterUsageTypeNanoseconds,   ///< Counter value is expressed in nanoseconds.
    kCounterUsageTypeLast           ///< Marker indicating last element.
} CounterUsageType;

/// @brief An enum to identify the GPA derived counters exposed in RGP.
typedef enum CounterIdentifier
{
    kCounterUnknown,         ///< Unknown counter (used for raw hardware counters).
    kCounterInstCacheHit,    ///< Instruction cache hit counter.
    kCounterScalarCacheHit,  ///< Scalar cache hit counter.
    kCounterL0CacheHit,      ///< L0 cache hit counter.
    kCounterL1CacheHit,      ///< L1 cache hit counter.
    kCounterL2CacheHit       ///< L2 cache hit counter.
} CounterIdentifier;

/// @brief Class providing access to SPM counter data. This class can support both raw counter data and GPA-derived counter data.
///
/// GPA derived counter data will be calculated on-demand if this class is asked for GPA derived counters (see the "derived"
/// parameter in GetNumberOfCounters, GetCounterName and GetCounterValue)
class RgpSpmDataBase
{
public:  // RDP needs access to all of these for now
    /// @brief A struct to hold information about a GPA derived counter.
    typedef struct DerivedCounterInfo
    {
        std::string       gpa_counter_name;       ///< The name of the counter as exposed by GPA.
        std::string       friendly_counter_name;  ///< User-friendly version of the counter name to display in the RGP UI.
        CounterIdentifier counter_id;             ///< An id associated with the counter.
    } DerivedCounterInfo;

    /// @brief A struct to hold info about a single counter component.
    typedef struct CounterComponent
    {
        std::string component_name;     ///< The component name.
        std::string component_formula;  ///< The formula to use to calculate the component value.
    } CounterComponent;

    /// @brief A struct to hold info about the components that make up a derived counter.
    typedef struct CounterComponentInfo
    {
        std::vector<CounterComponent> component_list;        ///< The list of component names.
        std::vector<std::string>      derived_counter_list;  ///< The list of derived counters whose value are used in calculating components.
    } CounterComponentInfo;

    /// @brief Structure for a generic progress callback.
    using ProgressCallback = std::function<void(size_t total, size_t completed)>;

public:
    /// @brief Constructor.
    RgpSpmDataBase();

    /// @brief Destructor.
    virtual ~RgpSpmDataBase();

    /// @brief Initialize SPM database, just assign RGP file buffer in memory to SPM database.
    ///
    /// @param [in] spm_db_chunk  A buffer containing a SPM database chunk.
    /// @param [in] asic_info The AsicInfo chunk of the .rgp file.
    /// @param [in] requested_derived_counters The list of enabled derived counters in the trace.
    /// @param [in] requested_counter_components A mapping of derived counters to their components.
    /// @param [in] filter_bad_spm_data Whether we should exclude SPM counters with negative values.
    /// @param [in] logger Object to use for logging.
    /// @retval
    /// kRgpOk The operation completed successfully.
    RgpErrorCode Initialize(const void*                                           spm_db_chunk,
                            RgpFileChunkAsicInfo                                  asic_info,
                            std::vector<DerivedCounterInfo>                       requested_derived_counters,
                            std::unordered_map<std::string, CounterComponentInfo> requested_counter_components,
                            bool                                                  filter_bad_spm_data,
                            const std::shared_ptr<devtrace::Logger>&              logger);

    /// @brief Initialize SPM database, just assign RGP file buffer in memory to SPM database.
    ///
    /// @param [in] chunk_file  The chunk file to initialize the database from.
    /// @param [in] asic_info The AsicInfo chunk of the .rgp file.
    /// @param [in] requested_derived_counters The list of enabled derived counters in the trace.
    /// @param [in] requested_counter_components A mapping of derived counters to their components.
    /// @param [in] filter_bad_spm_data Whether we should exclude SPM counters with negative values.
    /// @param [in] logger Object to use for logging.
    /// @retval
    /// kRgpOk The operation completed successfully.
    RgpErrorCode InitializeRdf(rdfChunkFile*                                         chunk_file,
                               RgpFileChunkAsicInfoRdfV3                             asic_info,
                               std::vector<DerivedCounterInfo>                       requested_derived_counters,
                               std::unordered_map<std::string, CounterComponentInfo> requested_counter_components,
                               bool                                                  filter_bad_spm_data,
                               const std::shared_ptr<devtrace::Logger>&              logger);

private:
    /// @brief Filters the expected_derived_counters_ array.
    /// @param [in] requested_counter_components A mapping of derived counters to their components.
    void FillExpectedCounters(const std::unordered_map<std::string, CounterComponentInfo>& requested_counter_components);

    /// @brief Maps the counter timestamps.
    void MapTimestamps();

public:
    /// @brief Gets sampling inteval.
    ///
    /// @retval The sampling interval.
    uint32_t GetSamplingInterval() const;

    /// @brief Gets the number of timestamps in the SPM database.
    ///
    /// @retval The number of timestamps in the the SPM database.
    size_t GetNumberOfTimestamps() const;

    /// @brief Gets the specified timestamps from the SPM database.
    ///
    /// @param [in]  timestamp_index The index of the timestamp.
    /// @param [out] timestamp       The timestamp at the specified index.
    ///
    /// @retval
    /// kRgpOk The operation completed successfully.
    RgpErrorCode GetTimestamp(size_t timestamp_index, uint64_t& timestamp) const;

    /// @brief Gets the number of counters in the SPM database.
    ///
    /// @param [in] derived Flag indicating if the count of derived counters (vs raw hardware counters) is desired.
    /// @param [in] progress_callback An optional callback to be used.
    /// @param [in] should_abort An optional boolean to periodically check if the get operation should be aborted.
    /// @retval The number of counters in the the SPM database.
    size_t GetNumberOfCounters(bool derived = true, const ProgressCallback& progress_callback = nullptr, const std::atomic_bool* should_abort = nullptr);

    /// @brief Gets the name of the specified counter.
    ///
    /// @param [in]  counter_index The index of the counter whose name is needed.
    /// @param [out] counter_name  The name of the specified counter.
    /// @param [in]  derived       Flag indicating if derived counter names (vs raw hardware counter names) are desired.
    ///
    /// @retval
    /// kRgpOk The operation completed successfully.
    RgpErrorCode GetCounterName(size_t counter_index, std::string& counter_name, bool derived = true);

    /// @brief Gets the GPA name of the specified counter.
    ///
    /// @param [in]  counter_index The index of the counter whose name is needed.
    /// @param [out] counter_name  The name of the specified counter.
    /// @param [in]  derived       Flag indicating if derived counter names (vs raw hardware counter names) are desired.
    ///
    /// @retval
    /// kRgpOk The operation completed successfully.
    RgpErrorCode GetGpaCounterName(size_t counter_index, std::string& counter_name, bool derived = true);

    /// Gets the description of the specified counter.
    ///
    /// @param [in]  counter_index The index of the counter whose description is needed.
    /// @param [out] counter_desc  The description of the specified counter.
    /// @param [in]  derived       Flag indicating if derived counter descriptions (vs raw hardware counter descriptions) are desired.
    ///
    /// @retval
    /// kRgpOk The operation completed successfully.
    RgpErrorCode GetCounterDescription(size_t counter_index, std::string& counter_desc, bool derived = true);

    /// @brief Gets the identifier of the specified counter.
    ///
    /// @param [in]  counter_index The index of the counter whose identifier is needed.
    /// @param [out] counter_id    The identifier of the specified counter.
    /// @param [in]  derived       Flag indicating if derived counter identifiers (vs raw hardware counter identifiers) are desired.
    ///
    /// @retval
    /// kRgpOk The operation completed successfully.
    RgpErrorCode GetCounterIdentifier(size_t counter_index, CounterIdentifier& counter_id, bool derived = true);

    /// @brief Gets the usage type of the specified counter.
    ///
    /// @param [in]  counter_index      The index of the counter whose usage type is needed.
    /// @param [out] counter_usage_type The usage type of the specified counter.
    /// @param [in]  derived            Flag indicating if derived counter usage types (vs raw hardware counter usage types) are desired.
    ///
    /// @retval
    /// kRgpOk The operation completed successfully.
    RgpErrorCode GetCounterUsageType(size_t counter_index, CounterUsageType& counter_usage_type, bool derived = true);

    /// @brief Gets the value of the specified counter at the specified timestamp.
    ///
    /// @param [in]  timestamp_index The index of the timestamp whose counter value is needed.
    /// @param [in]  counter_index   The index of the counter whose value is needed.
    /// @param [out] counter_value   The value of the specified counter.
    /// @param [in]  derived         Flag indicating if derived counter values (vs raw hardware counter values) are desired.
    ///
    /// @retval
    /// kRgpOk The operation completed successfully.
    RgpErrorCode GetCounterValue(size_t timestamp_index, size_t counter_index, double& counter_value, bool derived = true);

    /// @brief Gets the number of counter components for the specified counter. Counter components are displayed in the SPM tooltip in the UI.
    ///
    /// @param [in] counter_index  The index of the counter whose number of components is needed.
    /// @param [in] derived        Flag indicating if derived counter components are desired. Non-derived counters do not have components, so if non-dervied data is requested, this will return zero.
    ///
    /// @retval The number of components for the specified counter.
    size_t GetNumberOfCounterComponents(size_t counter_index, bool derived = true);

    /// @brief Gets the name of the specified component for the specified counter.
    ///
    /// @param [in]  counter_index   The index of the counter whose component name is needed.
    /// @param [in]  component_index The index of the the component whose name is needed.
    /// @param [out] component_name  The component name of the specified counter.
    /// @param [in]  derived         Flag indicating if derived counter component names are desired. Non-derived counters do not have components, so if non-dervied data is requested, this will return an empty string.
    ///
    /// @retval
    /// kRgpOk The operation completed successfully.
    RgpErrorCode GetCounterComponentName(size_t counter_index, size_t component_index, std::string& component_name, bool derived = true);

    /// @brief Gets the name of the specified component for the specified counter.
    ///
    /// @param [in]  counter_index   The index of the counter whose component name is needed.
    /// @param [in]  component_index The index of the the component whose name is needed.
    /// @param [out] component_description  The component description of the specified counter.
    /// @param [in]  derived         Flag indicating if derived counter component names are desired. Non-derived counters do not have components, so if non-dervied data is requested, this will return an empty string.
    ///
    /// @retval
    /// kRgpOk The operation completed successfully.
    RgpErrorCode GetCounterComponentDescription(size_t counter_index, size_t component_index, std::string& component_description, bool derived);

    /// @brief Gets the usage type of the specified component for the specified counter.
    ///
    /// @param [in]  counter_index   The index of the counter whose component name is needed.
    /// @param [in]  component_index The index of the the component whose name is needed.
    /// @param [out] usage_type  The component usage type of the specified counter.
    /// @param [in]  derived         Flag indicating if derived counter component names are desired. Non-derived counters do not have components, so if non-dervied data is requested, this will return an empty string.
    ///
    /// @retval
    /// kRgpOk The operation completed successfully.
    RgpErrorCode GetCounterComponentUsageType(size_t counter_index, size_t component_index, CounterUsageType& usage_type, bool derived);

    /// @brief Gets the value of the specified component for the specified timestamp and counter.
    ///
    /// @param [in]  timestamp_index The index of the timestamp whose component value is needed.
    /// @param [in]  counter_index   The index of the counter whose component value is needed.
    /// @param [in]  component_index The index of the component whose value is needed.
    /// @param [out] component_value The value of the specified component.
    /// @param [in]  derived         Flag indicating if derived counter component values are desired. Non-derived counters do not have components, so if non-dervied data is requested, this will reutrn a zero component value.
    ///
    /// @retval
    /// kRgpOk The operation completed successfully.
    RgpErrorCode GetCounterComponentValue(size_t timestamp_index, size_t counter_index, size_t component_index, double& component_value, bool derived = true);

    /// @brief Gets the raw SPM data. This data does not have the timestamps mapped.
    ///
    /// This is used by RGPFileAnalyzer to vend the raw SPM data to PIX.
    /// The SPM test cases in the backend test suite also access the raw SPM data.
    ///
    /// @retval Reference to the raw SPM data from the RGP file
    const RawRgpSpmDataBase& GetRawSpmDatabase() const;

    /// @brief Indicates if the specified timestamp index represents a valid timestamp.
    ///
    /// All counters associated with invalid timestamps are assigned a zero value.
    ///
    /// @param [in] timestamp_index The index of the timestamp to check for validity.
    ///
    /// @retval true if the timestamp is valid, false otherwise.
    bool IsTimestampValid(size_t timestamp_index) const;

private:
    /// @brief A struct to hold information about a counter.
    typedef struct CounterInfo
    {
        DerivedCounterInfo counter_info;        ///< The counter info (name, id, etc.).
        const std::string  counter_desc;        ///< The counter description.
        CounterUsageType   counter_usage_type;  ///< The counter usage type.
    } CounterInfo;

    /// @brief A struct to hold information about the expected derived counters.
    typedef struct ExpectedDerivedCounter
    {
        DerivedCounterInfo counter_info;          ///< The counter info (name, id, etc.).
        bool               is_component_counter;  ///< Flag indicating if the counter is a main counter or is used to compute a counter component.
        bool               is_available;  ///< Flag indicating if the counter is available (it is a GPA derived counter whose hardware counters are available).
    } ExpectedDerivedCounter;

    /// @brief An enum used to specify which parts of a counter info need to be queried.
    typedef enum CounterQueryType
    {
        kCounterQueryTypeName        = 0,  ///< Query the Counter name.
        kCounterQueryTypeDescription = 1,  ///< Query the counter description.
        kCounterQueryTypeGpaName     = 2,  ///< Query the gpa counter name.

        kCounterQueryTypeCount  ///< The number of counter info types that can be queried.
    } CounterQueryType;

    /// @brief A struct to hold information about a mapped SPM timestamp -- a mapped timestamp has been mapped to the same clock domain as SQTT.
    typedef struct MappedTimestamp
    {
        uint64_t timestamp;           ///< The mapped timestamp value.
        bool     timestamp_is_valid;  ///< Flag indicating if the timestamp is considered valid.
    } MappedTimestamp;

    typedef std::vector<double>                                   CounterValueList;     ///< Type for holding a list of counter values.
    typedef std::unordered_map<std::string, CounterComponentInfo> CounterComponentMap;  ///< Type to map a counter name to the component info for that counter.

    /// @brief Initializes the GPA function table and opens a GPA counter context.
    ///
    /// @retval
    /// kRgpOk The operation completed successfully.
    RgpErrorCode OpenGPACounterContext();

    /// @brief Helper function to calculate the derived counter values.
    ///
    /// @param [in] progress_callback An optional progress callback.
    /// @param [in] should_abort An optional boolean to check if the calculation should be aborted.
    ///
    /// @retval
    /// kRgpOk  The operation completed successfully.
    RgpErrorCode CalculateDerivedCounters(const ProgressCallback& progress_callback = {}, const std::atomic_bool* should_abort = nullptr);

    /// @brief Helper function to calculate a counter component value.
    ///
    /// @param [in] formula        String representation of the formula used to calculate this component.
    /// @param [in] counter_values List of derived counter values for the component.
    ///
    /// @return The calculated component value. Will be zero if an error occurs.
    static double CalculateComponentValue(const std::string& formula, CounterValueList& counter_values);

    /// @brief Helper function to get the specified piece of counter information from the specified counter
    ///
    /// @param [in]  counter_index The index of the counter whose information is needed.
    /// @param [in]  query_type    The piece of information needed for the specified counter.
    /// @param [in]  derived       Flag indicating if derived counter info (vs raw hardware counter info) is desired.
    /// @param [out] counter_info  String containing the specified piece of counter information - left unchanged if value other than kRgpOk is returned.
    ///
    /// @retval
    /// kRgpOk The operation completed successfully.
    RgpErrorCode GetCounterInfo(size_t counter_index, CounterQueryType query_type, bool derived, std::string& counter_info);

    /// @brief When derived counters with bad data have been filtered, we need to adjust the counter index when retrieving derived counter values.
    ///
    /// @param [in] counter_index The counter index to adjust.
    ///
    /// @return The adjusted counter index.
    size_t GetAdjustedIndex(size_t counter_index);

    bool derived_counters_calculated_ = false;  ///< Flag indicating if the derived counters have been calculated yet.
    bool counter_info_1_2_supported_  = false;  ///< Flag indicating if the SPM DB was generated with a new enough driver.
    bool counter_info_2_0_supported_  = false;  ///< Flag indicating if the SPM DB chunk is version 2.0 or newer.
    bool counter_info_3_0_supported_  = false;  ///< Flag indicating if the SPM DB chunk is version 3.0 or newer.
    bool filter_bad_spm_data_         = false;  ///< Flag indicating if bad SPM data should be filtered.

    std::vector<ExpectedDerivedCounter> expected_derived_counters_;     ///< Full list of derived counters (including component counters).
    std::vector<CounterInfo>            available_derived_counters_;    ///< List of derived counters we have data for.
    std::vector<CounterInfo>            available_counter_components_;  ///< List of counter components we have data for.

    std::vector<MappedTimestamp>  mapped_timestamps_;     ///< List of mapped timestamps.
    std::vector<CounterValueList> counter_values_;        ///< Vector of vector of counter_values: represents counter_values[counter_index][timestamp].
    std::vector<uint8_t>          counter_value_is_bad_;  ///< List determines which derived counters are bad ("bad" is a negative value).

    RawRgpSpmDataBase       raw_spm_db_;           ///< The raw spm database.
    GpaCounterLibFuncTable* gpa_func_table_;       ///< The GPA Counters lib function table.
    GpaCounterContext       gpa_counter_context_;  ///< The GPA counter context.

    /// @brief The asic info used by the database.
    struct DbAsicInfo
    {
        int32_t gfx_ip_level;  ///< The graphics IP level of the GPU.

        int32_t device_id;           ///< The device ID for the card where the profile was captured.
        int32_t device_revision_id;  ///< The device revision ID for the card where the profile was captured.

        int32_t shader_engines;                  ///< The number of shader engines.
        int32_t compute_unit_per_shader_engine;  ///< The number of compute units per shader engine.

        int32_t simd_per_compute_unit;  ///< The number of SIMDs per compute unit.
    };

    // Passed in by RDP
    std::shared_ptr<devtrace::Logger>                     logger_;
    DbAsicInfo                                            asic_info_;
    std::vector<DerivedCounterInfo>                       requested_derived_counters_;
    std::unordered_map<std::string, CounterComponentInfo> requested_counter_components_;
};

#endif
