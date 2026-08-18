// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team

#include "RdpCaptureApi.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

uint8_t ShouldAppConnect(void* user_data, const char* process_path, uint32_t process_id, RdpCaptureGpuApi api)
{
    // Can do anything in here really including an internal blocklist / allow list.
    if (strcmp(process_path, "C:\\Users\\developer\\Desktop\\cool.exe") == 0 && api == kRdpCaptureGpuApiDirectX12)
    {
        return 1;
    }

    return 0;
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

    RdpCaptureContextInitParams init_params = {};
    init_params.app_filter.userdata         = NULL;
    init_params.app_filter.filter           = &ShouldAppConnect;

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

    profiling_enable_params.body.profiling.flags = kRdpCaptureProfilingEnableParamFlagEnableShaderInstrumentation;

    if (capture_api.enable_feature(context, &profiling_enable_params) != kRdpCaptureResultSuccess)
    {
        printf("Failed to enable Profiling\n");
        return 1;
    }
    RdpCaptureFeatureEnableParams memory_trace_enable_params = {};
    memory_trace_enable_params.feature                       = kRdpCaptureFeatureMemoryTrace;

    memory_trace_enable_params.trace_finished_callback.user_data      = &captured_memory_trace;
    memory_trace_enable_params.trace_finished_callback.trace_finished = &TraceFinished;

    if (capture_api.enable_feature(context, &memory_trace_enable_params) != kRdpCaptureResultSuccess)
    {
        printf("Failed to enable Memory Trace\n");
        return 1;
    }

    capture_api.set_trace_desc(context, kRdpCaptureFeatureProfiling, "This capture was taken by the capture example from cool.exe");
    capture_api.set_trace_desc(context, kRdpCaptureFeatureMemoryTrace, "This capture was taken by the capture example from cool.exe");

    // Wait for a process to connect
    RdpCaptureProcessInfo process_info = {};
    do
    {
        capture_api.get_process_info(context, &process_info);

    } while (process_info.process_id == 0);

    // Wait for memory trace to start and for profiling to be ready since the process_info will be updated as soon as an app connects,
    // but won't wait until the app finishes initialization.
    do
    {
    } while (capture_api.get_feature_stage(context, kRdpCaptureFeatureProfiling) != kRdpCaptureFeatureStageReadyForCapture ||
             capture_api.get_feature_stage(context, kRdpCaptureFeatureMemoryTrace) != kRdpCaptureFeatureStageCapturing);

    RdpCaptureProfilingParams rgp_params = {};
    rgp_params.flags                     = kRdpCaptureProfilingParamFlagEnableInstructionTracing | kRdpCaptureProfilingParamFlagEnableCounterCollection;

    profiling_api.set_params(context, &rgp_params);

    RdpCaptureApiConnection* connections;
    uint64_t                 num_connections;

    capture_api.get_api_connections(context, kRdpCaptureFeatureProfiling, &connections, &num_connections);
    if (num_connections != 1 || connections[0].api != kRdpCaptureGpuApiDirectX12)
    {
        printf("Did not have the expected number / API connection\n");

        capture_api.free(connections);
        capture_api.destroy(context);

        return 1;
    }

    if (profiling_api.begin_trace(context, connections[0].id) != kRdpCaptureResultSuccess)
    {
        printf("Unable to start profiling capture\n");

        capture_api.free(connections);
        capture_api.destroy(context);

        return 1;
    }

    capture_api.free(connections);

    // Wait for both to finish asynchronously.
    while (captured_profiling == 0 && captured_memory_trace == 0)
    {
    }

    capture_api.destroy(context);

    return 0;
}
