// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Function table implementation for capture API.

#include "RdpCaptureApi.h"

#include <cstring>

#include "api_allocator.h"
#include "api_blocklist.h"
#include "capture_context.h"
#include "capture_params.h"

/// @brief This macro generates a function pointer via a lambda with no captures that can be used to call a member function on a capture context.
/// @param tbl The table to set the function on.
/// @param dest The function on global_table that this function pointer should serve as.
/// @param member The member function to call on RdpCaptureContextImpl.
#define CTX_FN(tbl, dest, member, ...) CTX_FN_DEFAULT(tbl, dest, member, , ##__VA_ARGS__)

/// @brief Version of CTX_FN that allows specifying a default return value if the context is null and additional params.
#define CTX_FN_DEFAULT(tbl, dest, member, def, ...)                                          \
    template <typename Ret, typename... Args>                                                \
    struct member##tbl##dst##Register                                                        \
    {                                                                                        \
        member##tbl##dst##Register(Ret (**func)(RdpCaptureContext, Args...))                 \
        {                                                                                    \
            *func = [](RdpCaptureContext ctx, Args... args) {                                \
                if (ctx == nullptr)                                                          \
                {                                                                            \
                    return def;                                                              \
                }                                                                            \
                                                                                             \
                RdpCaptureContextImpl* impl = reinterpret_cast<RdpCaptureContextImpl*>(ctx); \
                return impl->member(args..., ##__VA_ARGS__);                                 \
            };                                                                               \
            ;                                                                                \
        }                                                                                    \
    };                                                                                       \
    static member##tbl##dst##Register member##tbl##dst_reg(&tbl.dest);

static RdpCaptureFnTable global_table{};
CTX_FN_DEFAULT(global_table, enable_feature, EnableFeature, kRdpCaptureResultInvalidParams);
CTX_FN_DEFAULT(global_table, disable_feature, DisableFeature, kRdpCaptureResultInvalidParams);

CTX_FN(global_table, get_process_info, GetProcessInfo);

CTX_FN(global_table, get_gpu_clock_modes, GetGpuClockModes);
CTX_FN_DEFAULT(global_table, query_gpu_current_clock_mode, QueryGpuCurrentClockMode, kRdpCaptureResultInvalidParams);
CTX_FN_DEFAULT(global_table, set_gpu_current_clock_mode, SetCurrentGpuClockMode, kRdpCaptureResultInvalidParams);

CTX_FN_DEFAULT(global_table, get_feature_stage, GetFeatureStage, kRdpCaptureFeatureStageUnknown);
CTX_FN(global_table, get_api_connections, GetApiConnections);
CTX_FN(global_table, set_trace_desc, SetTraceDesc);

CTX_FN_DEFAULT(global_table, get_system_info, GetSystemInfo, kRdpCaptureResultInvalidParams);

static RdpCaptureFeatureProfilingFnTable profiling_table{};
CTX_FN(profiling_table, set_params, SetProfilingParams);
CTX_FN_DEFAULT(profiling_table, begin_trace, BeginTrace, kRdpCaptureResultInvalidParams, kRdpCaptureFeatureProfiling);
CTX_FN_DEFAULT(profiling_table, abort_trace, AbortTrace, kRdpCaptureResultInvalidParams, kRdpCaptureFeatureProfiling);

static RdpCaptureFeatureMemoryTraceFnTable memory_trace_table{};
CTX_FN_DEFAULT(memory_trace_table, insert_snapshot, InsertSnapshot, kRdpCaptureResultInvalidParams);
CTX_FN_DEFAULT(memory_trace_table, dump_trace, DumpTrace, kRdpCaptureResultInvalidParams);
CTX_FN_DEFAULT(memory_trace_table, abort_trace, AbortTrace, kRdpCaptureResultInvalidParams, kRdpCaptureFeatureMemoryTrace);

static RdpCaptureFeatureRaytracingFnTable raytracing_table{};
CTX_FN(raytracing_table, set_params, SetRaytracingParams);
CTX_FN_DEFAULT(raytracing_table, begin_trace, BeginTrace, kRdpCaptureResultInvalidParams, kRdpCaptureFeatureRaytracing);
CTX_FN_DEFAULT(raytracing_table, abort_trace, AbortTrace, kRdpCaptureResultInvalidParams, kRdpCaptureFeatureRaytracing);

static RdpCaptureFeatureCrashAnalysisFnTable crash_analysis_table{};
CTX_FN_DEFAULT(crash_analysis_table, abort_trace, AbortTrace, kRdpCaptureResultInvalidParams, kRdpCaptureFeatureCrashAnalysis);

static RdpCaptureBlocklistFnTable blocklist_table{};
CTX_FN_DEFAULT(blocklist_table, add_entry, AddBlocklistEntry, kRdpCaptureResultInvalidParams);
CTX_FN_DEFAULT(blocklist_table, remove_entry, RemoveBlocklistEntry, kRdpCaptureResultInvalidParams);
CTX_FN(blocklist_table, clear, ClearBlocklist);
CTX_FN(blocklist_table, get_entries, GetBlocklistEntries);
CTX_FN_DEFAULT(blocklist_table, load_file, LoadBlocklistFile, kRdpCaptureResultInvalidParams);

RdpCaptureContextImpl* Impl(RdpCaptureContext context)
{
    return reinterpret_cast<RdpCaptureContextImpl*>(context);
}

RdpCaptureResult Initialize(const RdpCaptureContextInitParams* init_params, RdpCaptureContext* out_context)
{
    if (init_params == nullptr || out_context == nullptr)
    {
        return kRdpCaptureResultInvalidParams;
    }

    RdpCaptureContextImpl* context     = new RdpCaptureContextImpl(*init_params);
    RdpCaptureResult       init_result = context->Initialize();

    if (init_result != kRdpCaptureResultSuccess)
    {
        delete context;
        return init_result;
    }

    *out_context = context;
    return kRdpCaptureResultSuccess;
}

void Destroy(RdpCaptureContext context)
{
    if (context == nullptr)
    {
        return;
    }

    delete Impl(context);
}

RDP_CAPTURE_API_EXPORT RdpCaptureResult RdpCaptureGetFnTable(uint32_t                  major_version,
                                                             [[maybe_unused]] uint32_t minor_version,
                                                             [[maybe_unused]] uint32_t patch_version,
                                                             RdpCaptureFnTable*        api_out)
{
    if (RDP_CAPTURE_API_VERSION_MAJOR != major_version)
    {
        return kRdpCaptureResultVersionMismatch;
    }

    if (api_out == nullptr)
    {
        return kRdpCaptureResultInvalidParams;
    }

    // Build a full local table with all fields populated.
    RdpCaptureFnTable local_table = global_table;

    local_table.profiling      = profiling_table;
    local_table.memory_trace   = memory_trace_table;
    local_table.raytracing     = raytracing_table;
    local_table.crash_analysis = crash_analysis_table;

    local_table.blocklist                      = blocklist_table;
    local_table.blocklist.free_entries         = &ApiBlocklist::FreeEntries;
    local_table.blocklist.set_blocked_callback = [](RdpCaptureContext ctx, void (*callback)(void*, const char*, uint32_t), void* user_data) {
        if (auto* impl = Impl(ctx))
        {
            impl->SetBlocklistBlockedCallback(callback, user_data);
        }
    };

    local_table.initialize = &Initialize;
    local_table.destroy    = &Destroy;
    local_table.free       = &ApiFree;

    local_table.free_system_info = &RdpCaptureContextImpl::FreeSystemInfo;

    local_table.profiling.get_default_params  = GetDefaultProfilingParams;
    local_table.raytracing.get_default_params = GetDefaultRaytracingParams;

    // Copy the full table to the caller.
    std::memcpy(api_out, &local_table, sizeof(RdpCaptureFnTable));

    return kRdpCaptureResultSuccess;
}
