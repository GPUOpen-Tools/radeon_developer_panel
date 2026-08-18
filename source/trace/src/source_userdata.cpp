// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for trace source structs.

#include "source_userdata.h"

#include <algorithm>

namespace devtrace
{
    bool RmvUserdata::operator==(const RmvUserdata& other) const
    {
        return output_path == other.output_path;
    }

    bool RgdUserdata::operator==(const RgdUserdata& other) const
    {
        return output_path == other.output_path && generate_text_summary == other.generate_text_summary &&
               generate_json_summary == other.generate_json_summary && summary_options.show_marker_source == other.summary_options.show_marker_source &&
               summary_options.expand_markers == other.summary_options.expand_markers &&
               summary_options.pdb_search_paths == other.summary_options.pdb_search_paths &&
               summary_options.pdb_include_subfolders == other.summary_options.pdb_include_subfolders && enable_advanced_crash == other.enable_advanced_crash &&
               disable_serialize_mem_ops == other.disable_serialize_mem_ops && disable_serialize_alu_ops == other.disable_serialize_alu_ops &&
               collect_wave_sgprs == other.collect_wave_sgprs && collect_wave_vgprs == other.collect_wave_vgprs;
    }

    bool DefaultCaptureMode::operator==(const DefaultCaptureMode& other) const
    {
        return application == other.application && api == other.api;
    }

    inline bool IsEqual(const TriggerableUserdata& lhs, const TriggerableUserdata& rhs)
    {
        return rhs.should_delay_capture == lhs.should_delay_capture && rhs.capture_delay == lhs.capture_delay &&
               rhs.shortcut_native_key == lhs.shortcut_native_key && rhs.shortcut_sequence == lhs.shortcut_sequence;
    }

    bool RgpUserdata::operator==(const RgpUserdata& other) const
    {
        if (default_capture_modes.size() != other.default_capture_modes.size())
        {
            return false;
        }

        for (const DefaultCaptureMode& mode : default_capture_modes)
        {
            // Usually the default operator== for DefaultCaptureMode only cares about matching application and api,
            // but here we want to ensure that the capture_mode also matches.
            auto it = std::ranges::find_if(other.default_capture_modes,
                                           [&](const DefaultCaptureMode& m) { return m == mode && m.capture_mode == mode.capture_mode; });
            if (it == other.default_capture_modes.end())
            {
                return false;
            }
        }

        return output_path == other.output_path && IsEqual(*this, other) && enable_inst_tracing == other.enable_inst_tracing &&
               enable_legacy_capture == other.enable_legacy_capture && enable_counters == other.enable_counters &&
               disable_capture_timeout == other.disable_capture_timeout && enable_exec_pop_tokens == other.enable_exec_pop_tokens &&
               sqtt_buffer_size_profile == other.sqtt_buffer_size_profile && custom_sqtt_buffer_size == other.custom_sqtt_buffer_size &&
               auto_capture_mode == other.auto_capture_mode && frame_capture_index == other.frame_capture_index &&
               dispatch_start_index == other.dispatch_start_index && dispatch_count == other.dispatch_count && draw_count == other.draw_count &&
               compute_auto_capture_time_ms == other.compute_auto_capture_time_ms && spm_counters_path == other.spm_counters_path &&
               spm_sampling_frequency == other.spm_sampling_frequency && enable_shader_instrumentation == other.enable_shader_instrumentation;
    }

    bool RraUserdata::operator==(const RraUserdata& other) const
    {
        return output_path == other.output_path && IsEqual(*this, other) && enable_ray_history == other.enable_ray_history &&
               ray_history_buffer_size == other.ray_history_buffer_size && ray_history_buffer_size_index == other.ray_history_buffer_size_index &&
               enable_marker_capture == other.enable_marker_capture && marker_begin_string == other.marker_begin_string &&
               marker_end_string == other.marker_end_string;
    }
}  // namespace devtrace
