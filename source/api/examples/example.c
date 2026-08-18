// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team

// Minimal pure C99 consumer of the public RdpCaptureApi.h header. Its purpose is
// to act as a compile gate: if this translation unit builds with a strict C99
// compiler, the public C API header is valid C99 and usable from C callers.

#include <stdio.h>

#include "RdpCaptureApi.h"

static RdpCaptureFnTable fn;

static uint8_t AppFilter(void* user_data, const RdpCaptureProcessInfo* process_info, RdpCaptureGpuApi api)
{
    (void)user_data;
    (void)process_info;
    return (uint8_t)(api == kRdpCaptureGpuApiDirectX12);
}

static void TraceFinished(void*                     user_data,
                          RdpCaptureFeature         feature,
                          RdpCaptureApiConnectionId connection,
                          RdpCaptureResult          result,
                          uint64_t                  size,
                          const uint8_t*            data)
{
    (void)user_data;
    (void)feature;
    (void)connection;
    (void)data;

    if (result != kRdpCaptureResultSuccess)
    {
        printf("Failed to capture trace\n");
        return;
    }

    printf("Trace finished: %llu bytes\n", (unsigned long long)size);
}

int main(int argc, char** argv)
{
    RdpCaptureContextInitParams   init_params      = {0};
    RdpCaptureContext             context          = NULL;
    RdpCaptureFeatureEnableParams feature_params   = {0};
    RdpCaptureProfilingParams     profiling_params = {0};
    RdpCaptureFeatureStage        stage;

    (void)argc;
    (void)argv;

    if (RdpCaptureGetFnTable(RDP_CAPTURE_API_VERSION_MAJOR, RDP_CAPTURE_API_VERSION_MINOR, RDP_CAPTURE_API_VERSION_PATCH, &fn) != kRdpCaptureResultSuccess)
    {
        return 1;
    }

    init_params.app_filter.filter = AppFilter;

    if (fn.initialize(&init_params, &context) != kRdpCaptureResultSuccess)
    {
        return 1;
    }

    feature_params.feature                                = kRdpCaptureFeatureProfiling;
    feature_params.trace_finished_callback.trace_finished = TraceFinished;
    if (fn.enable_feature(context, &feature_params) != kRdpCaptureResultSuccess)
    {
        fn.destroy(context);
        return 1;
    }

    fn.profiling.get_default_params(&profiling_params);
    fn.profiling.set_params(context, &profiling_params);

    stage = fn.get_feature_stage(context, kRdpCaptureFeatureProfiling, 0);
    if (stage == kRdpCaptureFeatureStageReadyForCapture)
    {
        fn.profiling.begin_trace(context, RDP_CAPTURE_API_FIRST_CONNECTION_ID);
    }

    fn.destroy(context);
    return 0;
}
