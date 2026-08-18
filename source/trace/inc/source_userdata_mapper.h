// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for trace source userdata mapper.

#ifndef RDP_SOURCE_TRACE_INC_SOURCE_USERDATA_MAPPER_H_
#define RDP_SOURCE_TRACE_INC_SOURCE_USERDATA_MAPPER_H_

#include <cstdint>

#include "dev_trace_common.h"
#include "json/json_mapper.h"
#include "source_userdata.h"
#include "tl/expected.hpp"

namespace devtrace
{
    static constexpr const char* kOutputPathKey = "output_path";  ///< JSON key for the output path (all sources).

    static constexpr const char* kShouldDelayCaptureKey = "delay_capture";  ///< JSON for enabling capture delay.
    static constexpr const char* kCaptureDelayKey       = "capture_delay";  ///< JSON key for capture delay.

    static constexpr const char* kShortcutSequenceKey  = "shortcut_sequence";    ///< JSON key for shortcut sequence.
    static constexpr const char* kShortcutNativeKeyKey = "shortcut_native_key";  ///< JSON for shortcut native key.

    static constexpr const char* kEnableInstTracingKey     = "enable_inst_tracing";      ///< JSON key for enabling instruction tracing.
    static constexpr const char* kEnableCountersKey        = "enable_counters";          ///< JSON key for enabling counters.
    static constexpr const char* kEnableLegacyCaptureKey   = "enable_legacy_capture";    ///< JSON key for enabling legacy capture.
    static constexpr const char* kDisableCaptureTimeoutKey = "disable_capture_timeout";  ///< JSON key for disabling capture timeout.
    static constexpr const char* kEnableExecPopCntKey      = "enable_exec_pop_tokens";   ///< JSON key for enabling exec pop tokens.

    static constexpr const char* kSqttBufferSizeProfile = "sqtt_buffer_size_profile";  ///< JSON key for the sqtt buffer size profile for profiling.
    static constexpr const char* kCustomSqttBufferSize  = "sqtt_buffer_custom_size";   ///< JSON key for the custom SQTT buffer size.

    static constexpr const char* kOpenClAutoTriggerKey = "opencl_auto_trigger";   ///< JSON key for the compute auto trigger for profiling.
    static constexpr const char* kDispatchStartKey     = "dispatch_start_index";  ///< JSON key for the compute dispatch start index for profiling.

    static constexpr const char* kDispatchCountKey = "dispatch_count";  ///< JSON key for the compute dispatch count for profiling.
    static constexpr const char* kDrawCountKey     = "draw_count";      ///< JSON key for the draw count for profiling.

    static constexpr const char* kFrameTriggerKey           = "frame_trigger";        ///< JSON key for the automatic frame capture for profiling.
    static constexpr const char* kFrameCaptureIndexKey      = "frame_capture_index";  ///< JSON key for the automatic frame capture index for profiling.
    static constexpr const char* kComputeAutoCaptureTimeKey = "compute_auto_capture_time_ms";  ///< JSON key for automatic dispatch capture time for profiling.

    static constexpr char const* kSpmCounterPathKey      = "spm_counter_path";       ///< JSON key for SPM counter path for profiling.
    static constexpr char const* kSpmCounterFrequencyKey = "spm_counter_frequency";  ///< JSON key for SPM sampling frequency for profiling.

    static constexpr const char* kDefaultModesKey    = "default_capture_modes";  ///< JSON key for default capture modes.
    static constexpr const char* kDefaultModeAppKey  = "app_name";               ///< JSON key for capture mode app name.
    static constexpr const char* kDefaultModeApiKey  = "api";                    ///< JSON key API of default capture mode.
    static constexpr const char* kDefaultModeModeKey = "default_mode";           ///< JSON key for the default capture mode.

    static constexpr const char* kEnableRayHistoryKey          = "enable_ray_history";              ///< JSON key for enabling ray history.
    static constexpr const char* kRayHistoryBufferCustomKey    = "ray_history_buffer_size_custom";  ///< JSON key for ray history custom buffer size.
    static constexpr const char* kRayHistoryBufferSizeIndexKey = "ray_history_buffer_size_index";   ///< JSON key for ray history size selection index.

    static constexpr const char* kEnableMarkerCaptureKey = "enable_marker_capture";  ///< JSON key for enabling marker-based capture.
    static constexpr const char* kMarkerBeginStringKey   = "marker_begin_string";    ///< JSON key for marker begin string.
    static constexpr const char* kMarkerEndStringKey     = "marker_end_string";      ///< JSON key for marker end string.

    static constexpr const char* kGenerateTextSummaryKey = "generate_text_summary";  ///< JSON key for text summary generation.
    static constexpr const char* kGenerateJsonSummaryKey = "generate_json_summary";  ///< JSON key for JSON summary generation.

    static constexpr const char* kShowMarkerSourceKey = "show_marker_source";  ///< JSON key for generate marker source.
    static constexpr const char* kExpandMarkerKey     = "expand_markers";      ///< JSON key for expand marker tree.

    static constexpr const char* kEnableEnhancedCrashKey = "enable_advanced_crash";   ///< JSON key for enhanced crash analysis.
    static constexpr const char* kSerializeMemOpsKey     = "serialize_mem_ops";       ///< JSON key for serialize memory operations.
    static constexpr const char* kSerializeAluOpsKey     = "serialize_alu_ops";       ///< JSON key for serialize ALU operations.
    static constexpr const char* kCollectWaveVgprSgpr    = "collect_wave_vgpr_sgpr";  ///< JSON key for collect wave vgpr and sgpr.
    static constexpr const char* kCollectWaveSgprsKey    = "collect_wave_sgprs";      ///< JSON key for collect wave sgprs.
    static constexpr const char* kCollectWaveVgprsKey    = "collect_wave_vgprs";      ///< JSON key for collect wave vgprs.

    static constexpr const char* kPdbSearchPathsKey       = "pdb_search_paths";        ///< JSON key for PDB search paths.
    static constexpr const char* kPdbIncludeSubfoldersKey = "pdb_include_subfolders";  ///< JSON key for PDB include subfolders.

    static constexpr char const* kShaderInstrumentationKey = "enable_shader_instrumentation";  ///< JSON key for shader instrumentation.

    /// @brief Base class to serialize / deserialize userdata to and from a struct.
    /// @tparam T The struct to put the userdata into.
    template <typename T>
    class UserdataMapper
    {
    public:
        using UserdataType = T;  ///< The type for the userdata struct.

        /// @brief Destructor.
        virtual ~UserdataMapper() = default;

        /// @brief Parses the userdata node.
        ///
        /// This assumes that data points to a UTF-8 string.
        /// @param [in] data The data for the userdata.
        /// @param [in] size The size in bytes of the data argument.
        /// @param [in] default_output_path The default output path for profiles (UTF-8).
        /// @param [out] out_data The output for the parsed data.
        /// @return void on success or an error message.
        virtual tl::expected<void, std::string> Parse(const void* data, size_t size, const std::string& default_output_path, T& out_data);

        /// @brief Serializes the userdata into a UTF-8 JSON string.
        /// @param [in] data The data to serialize to JSON.
        /// @return The serialized JSON string or an error message.
        virtual tl::expected<std::string, std::string> Serialize(T& data);

    protected:
        /// @brief Maps the JSON to / from the userdata.
        /// @param [in,out] data The data to write to the JSON or the place to read the data into.
        /// @param [in] mapper An object that can be used to read from the JSON and write to it.
        /// @param [in] The default output path.
        /// @return true if mapping was successful, false otherwise.
        virtual bool Map(UserdataType& data, JsonMapper& mapper, const std::string& default_output_path);
    };

    /// @brief Userdata mapper for RMV.
    using RmvUserdataMapper = UserdataMapper<RmvUserdata>;

    /// @brief Userdata mapper for RRA.
    class RraUserdataMapper : public UserdataMapper<RraUserdata>
    {
    public:
        ~RraUserdataMapper() override = default;

    protected:
        /// @brief Maps the JSON to / from the userdata.
        /// @param [in,out] data The data to write to the JSON or the place to read the data into.
        /// @param [in] mapper An object that can be used to read from the JSON and write to it.
        /// @param [in] The default output path.
        /// @return true if mapping was successful, false otherwise.
        bool Map(RraUserdata& data, JsonMapper& mapper, const std::string& default_output_path) override;
    };

    /// @brief Userdata mapper for RGD.
    class RgdUserdataMapper : public UserdataMapper<RgdUserdata>
    {
    public:
        /// @brief Destructor.
        ~RgdUserdataMapper() override = default;

    protected:
        /// @brief Maps the JSON to / from the userdata.
        /// @param [in,out] data The data to write to the JSON or the place to read the data into.
        /// @param [in] mapper An object that can be used to read from the JSON and write to it.
        /// @param [in] The default output path.
        /// @return true if mapping was successful, false otherwise.
        bool Map(RgdUserdata& data, JsonMapper& mapper, const std::string& default_output_path) override;
    };

    /// @brief Userdata mapper for RGP.
    class RgpUserdataMapper : public UserdataMapper<RgpUserdata>
    {
    public:
        /// @brief Destructor.
        ~RgpUserdataMapper() override = default;

    protected:
        /// @brief Maps the JSON to / from the userdata.
        /// @param [in,out] data The data to write to the JSON or the place to read the data into.
        /// @param [in] mapper An object that can be used to read from the JSON and write to it.
        /// @param [in] The default output path.
        /// @return true if mapping was successful, false otherwise.
        bool Map(RgpUserdata& data, JsonMapper& mapper, const std::string& default_output_path) override;
    };

    template <typename T>
    tl::expected<void, std::string> UserdataMapper<T>::Parse(const void* data, size_t size, const std::string& default_output_path, UserdataType& out_data)
    {
        if (data == nullptr || size == 0)
        {
            return tl::make_unexpected("Data is null or empty");
        }

        JsonMapper mapper = JsonMapper::Deserialize(std::span<const char>{reinterpret_cast<const char*>(data), size});
        if (!mapper.IsValid())
        {
            return tl::make_unexpected("Failed to parse userdata JSON");
        }

        if (!Map(out_data, mapper, default_output_path))
        {
            return tl::make_unexpected("Failed to map userdata");
        }

        return {};
    }

    template <typename T>
    bool UserdataMapper<T>::Map(UserdataType& data, JsonMapper& mapper, const std::string& default_output_path)
    {
        return mapper[kOutputPathKey](data.output_path, default_output_path);
    }

    template <typename T>
    tl::expected<std::string, std::string> UserdataMapper<T>::Serialize(T& data)
    {
        JsonMapper mapper = JsonMapper::Serialize();
        if (!Map(data, mapper, ""))
        {
            return tl::make_unexpected("Failed to map userdata for serialization");
        }

        return mapper.ToString();
    }

}  // namespace devtrace

#endif
