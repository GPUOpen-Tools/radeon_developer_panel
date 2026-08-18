// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for trace source factory.

#include "trace_source_factory.h"

#include "logging.h"
#include "rgd/rgd_trace_source_impl.h"
#include "rgp/rgp_trace_source_impl.h"
#include "rmv/rmv_trace_source_impl.h"
#include "rra/rra_trace_source_impl.h"
#include "trace_io.h"

namespace devtrace
{
    std::shared_ptr<RgpTraceSource> TraceSourceFactory::CreateRgpSource(const std::shared_ptr<ReadWriteStreamProvider>& stream_provider,
                                                                        const std::shared_ptr<OverlayManager>&          overlay_manager,
                                                                        const std::shared_ptr<SystemInfoCache>&         system_info_cache,
                                                                        const std::shared_ptr<AdditionalChunkWriter>&   addl_chunk_writer,
                                                                        const std::shared_ptr<RgpSpmCounterHandler>&    spm_counter_handler,
                                                                        const std::shared_ptr<DeviceClocksManager>&     device_clocks_manager,
                                                                        DDGpuProfilingApi*                              profiling_api,
                                                                        DDUberTraceApi*                                 ubertrace_api,
                                                                        const std::shared_ptr<UbertraceUserFactory>&    ubertrace_factory,
                                                                        DDDriverUtilsApi*                               driver_utils_api,
                                                                        std::shared_ptr<Logger>                         logger)
    {
        if (system_info_cache == nullptr || overlay_manager == nullptr || stream_provider == nullptr || profiling_api == nullptr || ubertrace_api == nullptr ||
            ubertrace_factory == nullptr || driver_utils_api == nullptr || spm_counter_handler == nullptr || device_clocks_manager == nullptr ||
            addl_chunk_writer == nullptr)
        {
            return {};
        }

        if (logger == nullptr)
        {
            logger = std::make_shared<NoopLogger>();
        }

        return std::make_shared<RgpTraceSourceImpl>(stream_provider,
                                                    overlay_manager,
                                                    system_info_cache,
                                                    addl_chunk_writer,
                                                    spm_counter_handler,
                                                    device_clocks_manager,
                                                    profiling_api,
                                                    ubertrace_api,
                                                    ubertrace_factory,
                                                    driver_utils_api,
                                                    logger);
    }

    std::shared_ptr<RraTraceSource> TraceSourceFactory::CreateRraSource(const std::shared_ptr<ReadWriteStreamProvider>& stream_provider,
                                                                        const std::shared_ptr<OverlayManager>&          overlay_manager,
                                                                        const std::shared_ptr<SystemInfoCache>&         system_info_cache,
                                                                        const std::shared_ptr<AdditionalChunkWriter>&   addl_chunk_writer,
                                                                        DDUberTraceApi*                                 uber_trace_api,
                                                                        const std::shared_ptr<UbertraceUserFactory>&    ubertrace_factory,
                                                                        std::shared_ptr<Logger>                         logger)
    {
        if (stream_provider == nullptr || overlay_manager == nullptr || system_info_cache == nullptr || uber_trace_api == nullptr ||
            ubertrace_factory == nullptr || addl_chunk_writer == nullptr)
        {
            return {};
        }

        // Use a noop logger so the trace source doesn't have to worry about a null logger.
        if (logger == nullptr)
        {
            logger = std::make_shared<NoopLogger>();
        }

        return std::make_shared<RraTraceSourceImpl>(
            stream_provider, overlay_manager, system_info_cache, addl_chunk_writer, uber_trace_api, ubertrace_factory, logger);
    }

    std::shared_ptr<RmvTraceSource> TraceSourceFactory::CreateRmvSource(const std::shared_ptr<ReadWriteStreamProvider>& stream_provider,
                                                                        const std::shared_ptr<OverlayManager>&          overlay_manager,
                                                                        const std::shared_ptr<SystemInfoCache>&         system_info_cache,
                                                                        const std::shared_ptr<AdditionalChunkWriter>&   addl_chunk_writer,
                                                                        DDMemoryTraceApi*                               memory_trace_api,
                                                                        std::shared_ptr<Logger>                         logger)
    {
        if (stream_provider == nullptr || overlay_manager == nullptr || system_info_cache == nullptr || addl_chunk_writer == nullptr ||
            memory_trace_api == nullptr)
        {
            return {};
        }

        // Use a noop logger so the trace source doesn't have to worry about a null logger.
        if (logger == nullptr)
        {
            logger = std::shared_ptr<Logger>(new NoopLogger());
        }

        return std::make_shared<RmvTraceSourceImpl>(
            stream_provider, overlay_manager, system_info_cache, addl_chunk_writer, memory_trace_api, std::move(logger));
    }

    std::shared_ptr<RgdTraceSource> TraceSourceFactory::CreateRgdSource(const std::shared_ptr<ReadWriteStreamProvider>& stream_provider,
                                                                        const std::shared_ptr<OverlayManager>&          overlay_manager,
                                                                        const std::shared_ptr<RgdSummaryGenerator>&     summary_generator,
                                                                        const std::shared_ptr<SystemInfoCache>&         system_info_cache,
                                                                        const std::shared_ptr<AdditionalChunkWriter>&   addl_chunk_writer,
                                                                        DDGpuDetectiveApi*                              gpu_detective_api,
                                                                        DDUberTraceApi*                                 ubertrace_api,
                                                                        const std::shared_ptr<UbertraceUserFactory>&    ubertrace_factory,
                                                                        DDEnhancedCrashInfoApi*                         enhanced_api,
                                                                        std::shared_ptr<Logger>                         logger)
    {
        if (stream_provider == nullptr || overlay_manager == nullptr || summary_generator == nullptr || system_info_cache == nullptr ||
            addl_chunk_writer == nullptr || gpu_detective_api == nullptr || ubertrace_api == nullptr || ubertrace_factory == nullptr || enhanced_api == nullptr)
        {
            return {};
        }

        // Use a noop logger so the trace source doesn't have to worry about a null logger.
        if (logger == nullptr)
        {
            logger = std::shared_ptr<Logger>(new NoopLogger());
        }

        return std::make_shared<RgdTraceSourceImpl>(stream_provider,
                                                    overlay_manager,
                                                    summary_generator,
                                                    system_info_cache,
                                                    addl_chunk_writer,
                                                    gpu_detective_api,
                                                    ubertrace_api,
                                                    ubertrace_factory,
                                                    enhanced_api,
                                                    logger);
    }

}  // namespace devtrace
