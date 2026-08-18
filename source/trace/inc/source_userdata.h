// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for trace source userdata structs.

#ifndef RDP_SOURCE_TRACE_INC_SOURCE_USERDATA_H_
#define RDP_SOURCE_TRACE_INC_SOURCE_USERDATA_H_

#include <cstdint>
#include <string>
#include <unordered_set>

#include "dev_trace_common.h"
#include "rgd_summary_options.h"

namespace devtrace
{
    /// @brief A default capture mode.
    struct DefaultCaptureMode
    {
        std::string application;                   ///< The name of the application the default is for.
        Api         api          = Api::kUnknown;  ///< The API of the application that the default is for.
        uint32_t    capture_mode = 0;              ///< The capture mode;

        /// @brief Equality operator.
        /// @param [in] other The default capture mode to compare against.
        /// @return true if the default capture modes are equal, false otherwise.
        bool operator==(const DefaultCaptureMode& other) const;
    };
}  // namespace devtrace

/// @brief Definition for the hash function of default capture mode.
template <>
struct std::hash<devtrace::DefaultCaptureMode>
{
    /// @brief Hash operator
    /// @param [in] mode The default mode to hash.
    /// @return The hash of the mode.
    size_t operator()(const devtrace::DefaultCaptureMode& mode) const noexcept
    {
        return std::hash<std::string>{}(mode.application) ^ static_cast<uint8_t>(mode.api);
    }
};  // namespace std

namespace devtrace
{
    /// @brief The different automatic capture mode.
    enum AutoCaptureMode : uint8_t
    {
        kAutoCaptureModeNone = 0,         ///< No auto capture.
        kAutoCaptureModeFrameIndex,       ///< Capture specific frame index.
        kAutoCaptureModeDispatchIndices,  ///< Capture between two dispatch indices.
        kAutoCaptureModeTimer             ///< Capture after a certain amount of time has elapsed.
    };

    /// @brief Userdata that has a delay.
    struct TriggerableUserdata
    {
        bool     should_delay_capture = false;  ///< true if the capture should be delayed, false otherwise.
        uint32_t capture_delay        = 100;    ///< The capture delay in milliseconds.

        int shortcut_sequence   = 0;  ///< The shortcut key sequence.
        int shortcut_native_key = 0;  ///< The native shortcut key (without modifiers).
    };

    /// @brief A parsed userdata node for RMV.
    struct RmvUserdata
    {
        std::string output_path;  ///< The path that RMV traces should be output to (UTF-8).

        /// @brief Checks if this userdata is equal to the other.
        /// @param other The userdata to compare this userdata to.
        /// @return true if the other userdata is the same as this userdata.
        bool operator==(const RmvUserdata& other) const;
    };

    /// @brief A parsed userdata node for RGD.
    struct RgdUserdata
    {
        std::string       output_path;                        ///< The path that RGD dumps should be output to (UTF-8).
        bool              generate_text_summary = true;       ///< true if a text version of the crash data summary should be generated.
        bool              generate_json_summary = false;      ///< true if a JSON version of the crash data summary should be generated.
        RgdSummaryOptions summary_options{};                  /// < The additional options for summary generation.
        bool              enable_advanced_crash     = true;   ///< true if advanced crash analysis should be enabled.
        bool              disable_serialize_mem_ops = false;  ///< true if memory ops should not be serialized with enhanced crash.
        bool              disable_serialize_alu_ops = false;  ///< true if alu ops should not be serialized with enhanced crash.
        bool              collect_wave_sgprs        = false;  ///< true to collect wave SGPRs (scalar general-purpose registers) during enhanced crash analysis.
        bool              collect_wave_vgprs        = false;  ///< true to collect wave VGPRs (vector general-purpose registers) during enhanced crash analysis.

        /// @brief Checks if this userdata is equal to the other.
        /// @param other The userdata to compare this userdata to.
        /// @return true if the other userdata is the same as this userdata.
        bool operator==(const RgdUserdata& other) const;
    };

    /// @brief The different built-in sizes for the SQTT buffer.
    enum class SqttBufferSizeProfiles : uint8_t
    {
        kMinimum = 0,
        kLow,
        kDefault,
        kHigh,
        kMaximum,
        kCustom
    };

    /// @brief A parsed userdata node for RGP.
    struct RgpUserdata : TriggerableUserdata
    {
        std::string            output_path;                       ///< The path that RGP dumps should be output to (UTF-8).
        bool                   enable_inst_tracing      = false;  ///< true if instruction tracing should be enabled, false otherwise.
        bool                   enable_counters          = true;   ///< true if counters should be enabled, false otherwise.
        bool                   enable_legacy_capture    = false;  ///< true if legacy capture protocol should be used.
        bool                   disable_capture_timeout  = false;  ///< true if the capture timeout should be disabled, false otherwise.
        SqttBufferSizeProfiles sqtt_buffer_size_profile = SqttBufferSizeProfiles::kDefault;  ///< The built-in profile to use for the SQTT buffer.
        uint32_t               custom_sqtt_buffer_size  = 0;                                 ///< The custom size of the SQTT buffer.
        AutoCaptureMode        auto_capture_mode        = kAutoCaptureModeNone;              ///< true if auto capturing is enabled.
        uint32_t    compute_auto_capture_time_ms = 0;  ///< If auto capture is set to kTiming, the number of milliseconds to wait before capturing a profile.
        uint32_t    dispatch_start_index         = 1;  ///< The start dispatch index to capture from a compute API during auto capture.
        uint32_t    dispatch_count               = 0;  ///< The number of dispatches to capture from a compute API.
        uint32_t    draw_count                   = 1;  ///< The number of draws to capture in draw mode.
        uint32_t    frame_capture_index          = 0;  ///< The frame index to capture when auto capture mode is frame index.
        std::string spm_counters_path;                 ///< The path to the SPM counters file with the counters to request during captures (UTF-8).
        uint32_t    spm_sampling_frequency;            ///< The frequency that SPM counters are sampled at.
        bool        enable_shader_instrumentation = false;             ///< true if shader instrumentation is enabled, false otherwise.
        bool        enable_exec_pop_tokens        = false;             ///< true if Exec/Pop count SQTT tokens should be enabled, false otherwise.
        std::unordered_set<DefaultCaptureMode> default_capture_modes;  ///< The default capture modes.

        /// @brief Checks if this userdata is equal to the other.
        /// @param other The userdata to compare this userdata to.
        /// @return true if the other userdata is the same as this userdata.
        bool operator==(const RgpUserdata& other) const;
    };

    /// @brief The different preset sizes for the ray history buffer.
    enum RayHistoryBufferIndex : uint32_t
    {
        kRayHistoryBufferIndexMinimumBuffer = 0,
        kRayHistoryBufferIndexLowBuffer,
        kRayHistoryBufferIndexDefaultBuffer,
        kRayHistoryBufferIndexHighBuffer,
        kRayHistoryBufferIndexMaximumBuffer
    };

    /// @brief A parsed userdata node for RRA.
    struct RraUserdata : TriggerableUserdata
    {
        std::string output_path;                ///< The path that RRA traces should be output to (UTF-8).
        bool        enable_ray_history = true;  ///< true if ray history collection should be enabled, false otherwise.
        std::string ray_history_buffer_size;    ///< The ray history buffer size (as uint64_t string)
        uint32_t    ray_history_buffer_size_index = kRayHistoryBufferIndexDefaultBuffer;  ///< The selected buffer size index

        bool        enable_marker_capture = false;             ///< true if marker-based capture should be used, false for frame-based.
        std::string marker_begin_string   = "RRABeginMarker";  ///< The marker string that starts the capture.
        std::string marker_end_string     = "RRAEndMarker";    ///< The marker string that ends the capture.

        /// @brief Checks if this userdata is equal to the other.
        /// @param other The userdata to compare this userdata to.
        /// @return true if the other userdata is the same as this userdata.
        bool operator==(const RraUserdata& other) const;
    };

};  // namespace devtrace

#endif
