// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for capture param handling.

#include "capture_params.h"

#include <algorithm>

#include <rgp_trace_source.h>
#include <rra_trace_source.h>

void GetDefaultProfilingParams(RdpCaptureProfilingParams* params)
{
    if (params == nullptr)
    {
        return;
    }

    params->flags            = 0;
    params->sqtt_buffer_size = kRdpCaptureSqttBufferSizeDefault;

    params->begin_frame_terminator_string = nullptr;
    params->end_frame_terminator_string   = nullptr;

    params->begin_frame_terminator_tag = 0;
    params->end_frame_terminator_tag   = 0;

    params->capture_mode    = kRdpCaptureProfilingCaptureModeDefault;
    params->render_op_count = 1;
}

void ApplyProfilingParams(const RdpCaptureProfilingParams* params, const std::shared_ptr<devtrace::RgpTraceSource>& source)
{
    if (params == nullptr)
    {
        return;
    }

    devtrace::RgpTraceSourceConfig& config = source->GetConfig();
    config.enable_spm_counters             = (params->flags & kRdpCaptureProfilingParamFlagEnableCounterCollection) != 0;
    config.enable_inst_tracing             = (params->flags & kRdpCaptureProfilingParamFlagEnableInstructionTracing) != 0;

    // These need to be properly implemented in devtrace.
    DEV_TRACE_ASSERT(params->begin_frame_terminator_string == nullptr);
    DEV_TRACE_ASSERT(params->end_frame_terminator_string == nullptr);

    DEV_TRACE_ASSERT(params->begin_frame_terminator_tag == 0);
    DEV_TRACE_ASSERT(params->end_frame_terminator_tag == 0);

    devtrace::SqttBufferSizeProfiles devtrace_profile;
    switch (params->sqtt_buffer_size)
    {
    case kRdpCaptureSqttBufferSizeMinimum:
        devtrace_profile = devtrace::SqttBufferSizeProfiles::kMinimum;
        break;
    case kRdpCaptureSqttBufferSizeLow:
        devtrace_profile = devtrace::SqttBufferSizeProfiles::kLow;
        break;
    case kRdpCaptureSqttBufferSizeHigh:
        devtrace_profile = devtrace::SqttBufferSizeProfiles::kHigh;
        break;
    case kRdpCaptureSqttBufferSizeMaximum:
        devtrace_profile = devtrace::SqttBufferSizeProfiles::kMaximum;
        break;
    default:
        devtrace_profile = devtrace::SqttBufferSizeProfiles::kDefault;
    }

    config.sqtt_memory_limit = devtrace::RgpTraceSourceConfig::kSqttProfileSizes[static_cast<uint32_t>(devtrace_profile)];

    switch (params->capture_mode)
    {
    case kRdpCaptureProfilingCaptureModeDraw:
        config.draw_count = params->render_op_count;
        break;
    case kRdpCaptureProfilingCaptureModeDispatch:
        config.dispatch_count = params->render_op_count;
        break;
    default:
        break;
    }
}

void GetDefaultRaytracingParams(RdpCaptureRaytracingParams* params)
{
    if (params == nullptr)
    {
        return;
    }

    params->ray_history_buffer_size = kRdpCaptureRayHistoryBufferSizeRayHistoryDisabled;
    params->enable_marker_capture   = 0;
    params->marker_begin_string     = "RRABeginMarker";
    params->marker_end_string       = "RRAEndMarker";
}

void ApplyRaytracingParams(const RdpCaptureRaytracingParams* params, const std::shared_ptr<devtrace::RraTraceSource>& source)
{
    if (params == nullptr)
    {
        return;
    }

    devtrace::RraTraceSourceConfig& config = source->GetConfig();
    config.enable_ray_history              = params->ray_history_buffer_size != kRdpCaptureRayHistoryBufferSizeRayHistoryDisabled;

    if (config.enable_ray_history)
    {
        const auto& buffer_sizes          = devtrace::RraTraceSourceConfig::kRayHistoryBufferSizes;
        size_t      adjusted_buffer_index = std::min(buffer_sizes.size() - 1, static_cast<size_t>(params->ray_history_buffer_size));

        char*    end;
        uint64_t buffer_size = strtoull(buffer_sizes[adjusted_buffer_index], &end, 10);
        DEV_TRACE_ASSERT(buffer_size != 0);

        config.ray_history_buffer_size = buffer_size;
    }

    // Apply marker capture settings
    config.enable_marker_capture = params->enable_marker_capture != 0;
    if (params->marker_begin_string != nullptr)
    {
        config.marker_begin_string = params->marker_begin_string;
    }
    if (params->marker_end_string != nullptr)
    {
        config.marker_end_string = params->marker_end_string;
    }
}
