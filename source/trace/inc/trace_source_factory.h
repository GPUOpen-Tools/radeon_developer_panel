// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for trace source factory.

#ifndef RDP_SOURCE_TRACE_INC_TRACE_SOURCE_FACTORY_H_
#define RDP_SOURCE_TRACE_INC_TRACE_SOURCE_FACTORY_H_

#include <memory>

#include <ddApi.h>

#include "logging.h"
#include "rgd_summary_generator.h"
#include "rgd_trace_source.h"
#include "rgp_trace_source.h"
#include "rmv_trace_source.h"
#include "rra_trace_source.h"
#include "source_userdata_mapper.h"
#include "system_info_cache.h"
#include "trace_io.h"

struct DDUberTraceApi;
struct DDMemoryTraceApi;
struct DDGpuDetectiveApi;
struct DDGpuProfilingApi;
struct DDDriverUtilsApi;
struct DDApiRegistry;
struct DDEnhancedCrashInfoApi;

namespace devtrace
{
    /// @brief Responsible for creating different trace sources.
    class TraceSourceFactory
    {
    public:
        /// @brief Generic function to create different trace sources based on type.
        /// @tparam T The trace source type.
        /// @tparam Args The viarable argument type
        /// @param stream_provider The stream provider.
        /// @param args The variadic arguments passed to type specific factory function.
        /// @return trace source on success or nullptr.
        template <typename T, typename... Args>
        static std::shared_ptr<T> CreateTraceSource(const std::shared_ptr<ReadWriteStreamProvider>& stream_provider, Args&&... args)
        {
            if (stream_provider == nullptr)
            {
                return nullptr;
            }

            if constexpr (std::is_same_v<T, RraTraceSource>)
            {
                return CreateRraSource(stream_provider, std::forward<Args>(args)...);
            }
            else if constexpr (std::is_same_v<T, RgpTraceSource>)
            {
                return CreateRgpSource(stream_provider, std::forward<Args>(args)...);
            }
            else if constexpr (std::is_same_v<T, RmvTraceSource>)
            {
                return CreateRmvSource(stream_provider, std::forward<Args>(args)...);
            }
            else if constexpr (std::is_same_v<T, RgdTraceSource>)
            {
                return CreateRgdSource(stream_provider, std::forward<Args>(args)...);
            }
            else
            {
                return nullptr;
            }
        }

        /// @brief Creates a new RGP trace source.
        /// @param [in] stream_provider An object used to acquire writable streams for outputting files to disk.
        /// @param [in] overlay_manager Manager for developer mode overlay.
        /// @param [in] system_info_cache An object used to cache the system info.
        /// @param [in] addl_chunk_writer The object that does additional chunk writing.
        /// @param [in] spm_counter_handler An object used to handle SPM counters.
        /// @param [in] device_clocks_manager The device clocks manager.
        /// @param [in] profiling_api The profiling API from devdriver registry.
        /// @param [in] ubertrace_api The UberTrace API from devdriver registry.
        /// @param [in] ubertrace_factory The UberTrace user factory.
        /// @param [in] driver_utils_api The driver utils API from devdriver registry.
        /// @param [in] logger An object used for debug logging.
        /// @return The new trace source.
        static std::shared_ptr<RgpTraceSource> CreateRgpSource(const std::shared_ptr<ReadWriteStreamProvider>&     stream_provider,
                                                               const std::shared_ptr<class OverlayManager>&        overlay_manager,
                                                               const std::shared_ptr<SystemInfoCache>&             system_info_cache,
                                                               const std::shared_ptr<class AdditionalChunkWriter>& addl_chunk_writer,
                                                               const std::shared_ptr<RgpSpmCounterHandler>&        spm_counter_handler,
                                                               const std::shared_ptr<class DeviceClocksManager>&   device_clocks_manager,
                                                               DDGpuProfilingApi*                                  profiling_api,
                                                               DDUberTraceApi*                                     ubertrace_api,
                                                               const std::shared_ptr<class UbertraceUserFactory>&  ubertrace_factory,
                                                               DDDriverUtilsApi*                                   driver_utils_api,
                                                               std::shared_ptr<Logger>                             logger = nullptr);

        /// @brief Creates a new RRA trace source.
        /// @param [in] stream_provider An object used to acquire writable streams for dumping files.
        /// @param [in] overlay_manager Manager for developer mode overlay.
        /// @param [in] system_info_cache An object used to cache the system info.
        /// @param [in] addl_chunk_writer The object that does additional chunk writing.
        /// @param [in] uber_trace_api The UberTrace API.
        /// @param [in] ubertrace_factory The UberTrace user factory.
        /// @param [in] logger Object used for debug logging.
        /// @return The new trace source.
        static std::shared_ptr<RraTraceSource> CreateRraSource(const std::shared_ptr<ReadWriteStreamProvider>&     stream_provider,
                                                               const std::shared_ptr<OverlayManager>&              overlay_manager,
                                                               const std::shared_ptr<SystemInfoCache>&             system_info_cache,
                                                               const std::shared_ptr<class AdditionalChunkWriter>& addl_chunk_writer,
                                                               DDUberTraceApi*                                     uber_trace_api,
                                                               const std::shared_ptr<class UbertraceUserFactory>&  ubertrace_factory,
                                                               std::shared_ptr<Logger>                             logger = nullptr);

        /// @brief Creates a new RMV trace source.
        /// @param [in] stream_provider An object used to acquire writable streams for dumping files.
        /// @param [in] overlay_manager Manager for developer mode overlay.
        /// @param [in] system_info_cache An object used to cache the system info.
        /// @param [in] addl_chunk_writer The object that does additional chunk writing.
        /// @param [in] logger A logger to be used by the trace source.
        /// @param [in] memory_trace_api The memory trace API.
        /// @return The new trace source.
        static std::shared_ptr<RmvTraceSource> CreateRmvSource(const std::shared_ptr<ReadWriteStreamProvider>&     stream_provider,
                                                               const std::shared_ptr<OverlayManager>&              overlay_manager,
                                                               const std::shared_ptr<SystemInfoCache>&             system_info_cache,
                                                               const std::shared_ptr<class AdditionalChunkWriter>& addl_chunk_writer,
                                                               DDMemoryTraceApi*                                   memory_trace_api,
                                                               std::shared_ptr<Logger>                             logger = nullptr);

        /// @brief Creates a new RGD trace source.
        /// @param [in] stream_provider An object used to acquire read / write streams for dumping files.
        /// @param [in] overlay_manager Manager for developer mode overlay.
        /// @param [in] summary_generator An object that can be used to generate crash summaries.
        /// @param [in] system_info_cache An object used to cache the system info.
        /// @param [in] addl_chunk_writer The object that does additional chunk writing.
        /// @param [in] gpu_detective_api The Crash Analysis API.
        /// @param [in] enhanced_api The enhanced crash info API.
        /// @param [in] ubertrace_api The UberTrace API from devdriver registry.
        /// @param [in] ubertrace_factory The UberTrace user factory.
        /// @param [in] logger A logger to be used by the trace source.
        /// @return The new trace source.
        static std::shared_ptr<RgdTraceSource> CreateRgdSource(const std::shared_ptr<ReadWriteStreamProvider>&     stream_provider,
                                                               const std::shared_ptr<OverlayManager>&              overlay_manager,
                                                               const std::shared_ptr<RgdSummaryGenerator>&         summary_generator,
                                                               const std::shared_ptr<SystemInfoCache>&             system_info_cache,
                                                               const std::shared_ptr<class AdditionalChunkWriter>& addl_chunk_writer,
                                                               DDGpuDetectiveApi*                                  gpu_detective_api,
                                                               DDUberTraceApi*                                     ubertrace_api,
                                                               const std::shared_ptr<class UbertraceUserFactory>&  ubertrace_factory,
                                                               DDEnhancedCrashInfoApi*                             enhanced_api,
                                                               std::shared_ptr<Logger>                             logger = nullptr);
    };
}  // namespace devtrace

#endif
