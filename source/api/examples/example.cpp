//=============================================================================
// Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
//=============================================================================

#include <atomic>
#include <thread>

#include "RdpCaptureApi.h"

#include <dev_trace_common.h>

RdpCaptureFnTable fn;

std::atomic_bool finished = false;

static void TraceFinished([[maybe_unused]] void*                     user_data,
                          [[maybe_unused]] RdpCaptureFeature         feature,
                          [[maybe_unused]] RdpCaptureApiConnectionId conn_id,
                          RdpCaptureResult                           result,
                          [[maybe_unused]] uint64_t                  size,
                          [[maybe_unused]] const uint8_t*            data)
{
    if (result != kRdpCaptureResultSuccess)
    {
        printf("Failed to capture trace\n");

        return;
    }

    finished = true;
}

// This is a temp file to test
int main([[maybe_unused]] int argc, [[maybe_unused]] const char** argv)
{
    if (RdpCaptureGetFnTable(RDP_CAPTURE_API_VERSION_MAJOR, RDP_CAPTURE_API_VERSION_MINOR, RDP_CAPTURE_API_VERSION_PATCH, &fn) != kRdpCaptureResultSuccess)
    {
        return -1;
    }

    RdpCaptureContextInitParams params{};
    params.log_callback.log =
        []([[maybe_unused]] void* user_data, [[maybe_unused]] RdpCaptureLogLevel level, const char* source, uint32_t process_id, const char* msg) {
            std::cout << "[" << source << "] (PID " << process_id << ") " << msg << std::endl;
        };

    params.app_filter.filter = []([[maybe_unused]] void*                        user_data,
                                  [[maybe_unused]] const RdpCaptureProcessInfo* process_info,
                                  RdpCaptureGpuApi                              api) -> uint8_t { return api == kRdpCaptureGpuApiDirectX12; };

    RdpCaptureContext context{};
    if (fn.initialize(&params, &context) != kRdpCaptureResultSuccess)
    {
        return -1;
    }

    RdpCaptureSystemInfo system_info{};
    if (fn.get_system_info(context, &system_info) != kRdpCaptureResultSuccess)
    {
        return -1;
    }

    const uint64_t num_gpus = system_info.num_gpus;

    uint64_t                       num_details;
    RdpCaptureGpuClockModeDetails* details;

    int gpu_loop_result = 0;
    for (uint64_t idx = 0; idx < num_gpus; ++idx)
    {
        fn.get_gpu_clock_modes(context, idx, &details, &num_details);

        RdpCaptureGpuClockMode mode;
        if (fn.query_gpu_current_clock_mode(context, idx, &mode) != kRdpCaptureResultSuccess)
        {
            fn.free(details);
            gpu_loop_result = -1;
            break;
        }

        if (fn.set_gpu_current_clock_mode(context, idx, kRdpCaptureGpuClocksModeStable) != kRdpCaptureResultSuccess)
        {
            fn.free(details);
            gpu_loop_result = -1;
            break;
        }

        fn.free(details);
    }

    fn.free_system_info(&system_info);
    if (gpu_loop_result != 0)
    {
        return gpu_loop_result;
    }

    RdpCaptureFeatureEnableParams init_params          = {};
    init_params.feature                                = kRdpCaptureFeatureCrashAnalysis;
    init_params.trace_finished_callback.user_data      = nullptr;
    init_params.trace_finished_callback.trace_finished = TraceFinished;

    init_params.body.crash_analysis.flags |= kRdpCaptureCrashAnalysisEnableParamFlagEnableEnhancedCrashAnalysis;

    if (fn.enable_feature(context, &init_params) != kRdpCaptureResultSuccess)
    {
        return -1;
    }

    printf("Connect application\n");

    do
    {
    } while (fn.get_feature_stage(context, kRdpCaptureFeatureProfiling, 0) != kRdpCaptureFeatureStageCapturing);

    while (!finished)
    {
    }

    fn.destroy(context);
}
