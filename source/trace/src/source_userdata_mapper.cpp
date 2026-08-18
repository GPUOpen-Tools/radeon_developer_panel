// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for trace source userdata parsing.

#include "source_userdata_mapper.h"

#include <cstdint>

namespace devtrace
{
    inline bool MapTriggerableUserdata(TriggerableUserdata& data, JsonMapper& mapper)
    {
        return mapper[kShouldDelayCaptureKey](data.should_delay_capture, false) && mapper[kCaptureDelayKey](data.capture_delay, 100) &&
               mapper[kShortcutSequenceKey](data.shortcut_sequence, 0) && mapper[kShortcutNativeKeyKey](data.shortcut_native_key, 0);
    }

    bool RraUserdataMapper::Map(RraUserdata& data, JsonMapper& mapper, const std::string& default_output_path)
    {
        return UserdataMapper<RraUserdata>::Map(data, mapper, default_output_path) && MapTriggerableUserdata(data, mapper) &&
               mapper[kEnableRayHistoryKey](data.enable_ray_history, true) && mapper[kRayHistoryBufferCustomKey](data.ray_history_buffer_size, "0") &&
               mapper[kRayHistoryBufferSizeIndexKey](data.ray_history_buffer_size_index, RayHistoryBufferIndex::kRayHistoryBufferIndexDefaultBuffer) &&
               mapper[kEnableMarkerCaptureKey](data.enable_marker_capture, false) &&
               mapper[kMarkerBeginStringKey](data.marker_begin_string, "RRABeginMarker") && mapper[kMarkerEndStringKey](data.marker_end_string, "RRAEndMarker");
    }

    bool MapDefaultCaptureMode(DefaultCaptureMode& mode, JsonMapper mapper)
    {
        return mapper[kDefaultModeAppKey](mode.application) && mapper[kDefaultModeApiKey](mode.api) && mapper[kDefaultModeModeKey](mode.capture_mode);
    }

    bool RgpUserdataMapper::Map(RgpUserdata& data, JsonMapper& mapper, const std::string& default_output_path)
    {
        return UserdataMapper::Map(data, mapper, default_output_path) && MapTriggerableUserdata(data, mapper) &&
               mapper[kEnableInstTracingKey](data.enable_inst_tracing, false) && mapper[kEnableCountersKey](data.enable_counters, true) &&
               mapper[kEnableLegacyCaptureKey](data.enable_legacy_capture, false) && mapper[kDisableCaptureTimeoutKey](data.disable_capture_timeout, false) &&
               mapper[kEnableExecPopCntKey](data.enable_exec_pop_tokens, false) &&
               mapper[kSqttBufferSizeProfile](data.sqtt_buffer_size_profile, SqttBufferSizeProfiles::kDefault) &&
               mapper[kCustomSqttBufferSize](data.custom_sqtt_buffer_size, 0) &&
               mapper[kOpenClAutoTriggerKey](*reinterpret_cast<uint8_t*>(&data.auto_capture_mode), kAutoCaptureModeNone) &&
               mapper[kFrameCaptureIndexKey](data.frame_capture_index, 0) && mapper[kDispatchStartKey](data.dispatch_start_index, 1) &&
               mapper[kDispatchCountKey](data.dispatch_count, 0) && mapper[kDrawCountKey](data.draw_count, 1) &&
               mapper[kComputeAutoCaptureTimeKey](data.compute_auto_capture_time_ms, 0) && mapper[kSpmCounterPathKey](data.spm_counters_path, "") &&
               mapper[kSpmCounterFrequencyKey](data.spm_sampling_frequency, 0) &&
               mapper[kShaderInstrumentationKey](data.enable_shader_instrumentation, false) &&
               mapper[kDefaultModesKey].Set<DefaultCaptureMode>(data.default_capture_modes, MapDefaultCaptureMode, {});
    }

    bool RgdUserdataMapper::Map(RgdUserdata& data, JsonMapper& mapper, const std::string& default_output_path)
    {
        return UserdataMapper::Map(data, mapper, default_output_path) && mapper[kGenerateTextSummaryKey](data.generate_text_summary, false) &&
               mapper[kGenerateJsonSummaryKey](data.generate_json_summary, false) &&
               mapper[kShowMarkerSourceKey](data.summary_options.show_marker_source, false) &&
               mapper[kExpandMarkerKey](data.summary_options.expand_markers, false) && mapper[kEnableEnhancedCrashKey](data.enable_advanced_crash, true) &&
               mapper[kPdbSearchPathsKey].Arr<std::string>(
                   data.summary_options.pdb_search_paths, [](std::string& str, JsonMapper mapper) { return mapper(str); }, {}) &&
               mapper[kPdbIncludeSubfoldersKey](data.summary_options.pdb_include_subfolders, false) &&
               mapper[kSerializeMemOpsKey](data.disable_serialize_mem_ops, false) && mapper[kSerializeAluOpsKey](data.disable_serialize_alu_ops, false) &&
               mapper[kCollectWaveSgprsKey](data.collect_wave_sgprs, false) && mapper[kCollectWaveVgprsKey](data.collect_wave_vgprs, false);
    }
};  // namespace devtrace
