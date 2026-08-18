// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team

#include "RdpCaptureApi.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <processthreadsapi.h>
#else
#include <unistd.h>
#endif

static void TraceFinished(void* user_data, RdpCaptureFeature feature, RdpCaptureResult result, uint64_t size, const uint8_t* data)
{
    if (result != kRdpCaptureResultSuccess)
    {
        printf("Failed to capture trace\n");
        *((uint8_t*)user_data) = 1;

        return;
    }

    // Use data and size to write out to disk...

    *((uint8_t*)user_data) = 1;
}

int main(int argc, char** argv)
{
    RdpCaptureFnTable                 capture_api   = {};
    RdpCaptureFeatureProfilingFnTable profiling_api = {};

    if (RdpCaptureGetFnTable(RDP_CAPTURE_API_VERSION_MAJOR, RDP_CAPTURE_API_VERSION_MINOR, RDP_CAPTURE_API_VERSION_PATCH, &capture_api) !=
            kRdpCaptureResultSuccess ||
        capture_api.get_profiling_feature(&profiling_api) != kRdpCaptureResultSuccess)
    {
        printf("Failed to get functions tables.\n");
        return 1;
    }

    // Use a NULL filter because by default that will connect to the current process.
    RdpCaptureContextInitParams init_params = {};
    init_params.app_filter.userdata         = NULL;
    init_params.app_filter.filter           = NULL;

    RdpCaptureContext context;
    if (capture_api.initialize(&init_params, &context) != kRdpCaptureResultSuccess)
    {
        printf("Failed to initialize capture API\n");
        return 1;
    }

    uint8_t captured_profiling    = 0;
    uint8_t captured_memory_trace = 0;

    RdpCaptureFeatureEnableParams profiling_enable_params = {};
    profiling_enable_params.feature                       = kRdpCaptureFeatureProfiling;

    profiling_enable_params.trace_finished_callback.user_data      = &captured_profiling;
    profiling_enable_params.trace_finished_callback.trace_finished = &TraceFinished;

    // Capture an RGP profile for frame 150
    profiling_enable_params.body.profiling.frame_capture_index = 150;
    profiling_enable_params.body.profiling.flags =
        kRdpCaptureProfilingEnableParamFlagUseFrameIndexCapture | kRdpCaptureProfilingEnableParamFlagEnableShaderInstrumentation;

    if (capture_api.enable_feature(context, &profiling_enable_params) != kRdpCaptureResultSuccess)
    {
        printf("Failed to enable Profiling\n");
        return 1;
    }

    RdpCaptureFeatureEnableParams memory_trace_init_params = {};
    memory_trace_init_params.feature                       = kRdpCaptureFeatureMemoryTrace;

    memory_trace_init_params.trace_finished_callback.user_data      = &captured_memory_trace;
    memory_trace_init_params.trace_finished_callback.trace_finished = &TraceFinished;

    if (capture_api.enable_feature(context, &memory_trace_init_params) != kRdpCaptureResultSuccess)
    {
        printf("Failed to enable Memory Trace\n");
        return 1;
    }

    // Create GPU device...

    RdpCaptureProcessInfo process_info = {};
    capture_api.get_process_info(context, &process_info);

    if (process_info.process_id == 0)
    {
        printf("Failed to connect to current process\n");
        return 1;
    }

    if (capture_api.get_feature_stage(context, kRdpCaptureFeatureProfiling) != kRdpCaptureFeatureStageCapturing)
    {
        printf("Auto capture for RGP failed to start\n");
        return 1;
    }

    if (capture_api.get_feature_stage(context, kRdpCaptureFeatureMemoryTrace) != kRdpCaptureFeatureStageCapturing)
    {
        printf("Memory trace failed to start\n");
        return 1;
    }

    // RGP params just need to be set before frame 150 is rendered
    RdpCaptureProfilingParams rgp_params = {};
    rgp_params.flags                     = kRdpCaptureProfilingParamFlagEnableInstructionTracing | kRdpCaptureProfilingParamFlagEnableCounterCollection;

    profiling_api.set_params(context, &rgp_params);

    // Keep rendering, say 200 frames total
    // Destroy device

    // Wait for both to finish asynchronously.
    while (captured_profiling == 0 && captured_memory_trace == 0)
    {
    }

    capture_api.destroy(context);

    return 0;
}
