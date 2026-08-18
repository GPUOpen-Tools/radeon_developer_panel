// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for concrete implementation of the RGP trace source.

#ifndef RDP_SOURCE_TRACE_SRC_RGP_RGP_TRACE_SOURCE_IMPL_H_
#define RDP_SOURCE_TRACE_SRC_RGP_RGP_TRACE_SOURCE_IMPL_H_

#include <atomic>
#include <memory>
#include <thread>

#include <dd_driver_utils_api.h>
#include <dd_gpu_profiling_api.h>
#include <dev_trace_common.h>
#include <system_info_reader.h>

#include "../base_trace_source/base_triggerable_trace_source.h"
#include "../base_trace_source/trace_source_client.h"

#include "../base_trace_source/ubertrace_features.h"
#include "dipper.h"
#include "logging.h"
#include "rgp_trace_client.h"
#include "rgp_trace_source.h"
#include "standard_timer.h"
#include "system_info_cache.h"

struct DDGpuProfilingApi;
struct DDUberTraceApi;
struct DDDriverUtilsApi;

namespace devtrace
{
    /// @brief Factory for creating RGP clients.
    class RgpClientFactory : public ClientFactory<RgpClient>
    {
    public:
        /// @brief Constructor.
        /// @param [in] profiling_api The profiling API.
        /// @param [in] ubertrace_api The UberTrace API.
        /// @param [in] ubertrace_factory The UberTrace user factory.
        /// @param [in] addl_chunk_writer Object used for system info writing.
        /// @param [in] driver_utils_api The driver utils API from devdriver registry.
        /// @param [in] counter_handler Utility for handling counters.
        /// @param [in] device_clocks_manager The device clocks manager.
        RgpClientFactory(DDGpuProfilingApi*                                  profiling_api,
                         DDUberTraceApi*                                     ubertrace_api,
                         const std::shared_ptr<class UbertraceUserFactory>&  ubertrace_factory,
                         const std::shared_ptr<class AdditionalChunkWriter>& addl_chunk_writer,
                         DDDriverUtilsApi*                                   driver_utils_api,
                         const std::shared_ptr<RgpSpmCounterHandler>&        counter_handler,
                         const std::shared_ptr<class DeviceClocksManager>&   device_clocks_manager);

        virtual ~RgpClientFactory();

        std::unique_ptr<RgpClient> CreateClient(const ClientConnection& info, ClientUtils<RgpTraceSourceConfigPrivate>& client_utils) override;

        /// @brief Sets the GPUs for the system.
        /// @param [in] gpus The GPUs for the system.
        void SetGpus(const std::vector<system_info_utils::GpuInfo>& gpus);

        /// @brief Sets the GPUs in the system that support SPM capture.
        /// @param [in] spm_supported_gpus The GPUs in the system which support SPM capture.
        void SetSpmSupportedGpus(const std::vector<system_info_utils::GpuInfo>& spm_supported_gpus);

        /// @brief Set the driver info for client factory
        /// @param [in] driver The driver info struct
        void SetDriver(const system_info_utils::DriverInfo& driver);

        /// @brief Sets the UberTrace features.
        /// @param [in] features The UberTrace driver features.
        void SetUbertraceFeatures(const UbertraceFeatures& features) const;

    private:
        DDGpuProfilingApi*                           profiling_api_ = nullptr;     ///< The profiling API.
        DDUberTraceApi*                              ubertrace_api_ = nullptr;     ///< The UberTrace API.
        std::shared_ptr<UbertraceUserFactory>        ubertrace_factory_;           ///<  The UberTrace user factory.
        std::shared_ptr<class AdditionalChunkWriter> addl_chunk_writer_;           ///< Object used for system info writing.
        DDDriverUtilsApi*                            driver_utils_api_ = nullptr;  ///< The driver utils API from devdriver registry.
        std::shared_ptr<RgpSpmCounterHandler>        counter_handler_;             ///< Utility for handling counters.
        std::shared_ptr<class DeviceClocksManager>   device_clocks_manager_;       ///< The device clocks manager.
        system_info_utils::DriverInfo                driver_info_;                 ///< Driver info
        std::vector<system_info_utils::GpuInfo>      gpus_;                        ///< The GPUs currently in the system.
        std::vector<system_info_utils::GpuInfo>      spm_supported_gpus_;          ///< The GPUs in the system with support for SPM capture.
    };

    /// @brief Concrete implementation of RGP trace source.
    class RgpTraceSourceImpl final : public RgpTraceSource, public IClientEventBindingDelegate<RgpClient>
    {
    public:
        static void OnTraceCompletionEventCallback(void* listener, const devtrace::TraceCompletionEventArgs& args);

        /// @brief Constructor.
        /// @param [in] stream_provider An object used to acquire writable streams for dumping files.
        /// @param [in] overlay_manager Manager for developer mode overlay.
        /// @param [in] system_info_cache A cache of the system information.
        /// @param [in] addl_chunk_writer The object that does additional chunk writing.
        /// @param [in] spm_counter_handler An object used to handle SPM counters.
        /// @param [in] device_clocks_manager The device clocks manager.
        /// @param [in] profiling_api The profiling API from devdriver registry.
        /// @param [in] ubertrace_factory The UberTrace user factory.
        /// @param [in] ubertrace_api The UberTrace API.
        /// @param [in] driver_utils_api The driver utils API from devdriver registry.
        /// @param [in] logger Object used for debug logging.
        DIP(RgpTraceSourceImpl(const std::shared_ptr<ReadWriteStreamProvider>&    stream_provider,
                               const std::shared_ptr<OverlayManager>&             overlay_manager,
                               const std::shared_ptr<SystemInfoCache>&            system_info_cache,
                               const std::shared_ptr<AdditionalChunkWriter>&      addl_chunk_writer,
                               const std::shared_ptr<RgpSpmCounterHandler>&       spm_counter_handler,
                               const std::shared_ptr<class DeviceClocksManager>&  device_clocks_manager,
                               DDGpuProfilingApi*                                 profiling_api,
                               DDUberTraceApi*                                    ubertrace_api,
                               const std::shared_ptr<class UbertraceUserFactory>& ubertrace_factory,
                               DDDriverUtilsApi*                                  driver_utils_api,
                               const std::shared_ptr<Logger>&                     logger));

        void RouterConnectionStatusChanged(bool is_connected) override;
        void OnDriverConnected(const DDConnectionInfo& connection_info) override;
        void OnDriverDisconnected(DDConnectionId umd_connection_id) override;
        void OnDriverStateChanged(DDConnectionId umd_connection_id, DD_DRIVER_STATE state) override;

        Result RequestAbortTrace(DDConnectionId umd_connection_id) override;
        Result RequestAbortProcessing() override;

        void RegisterSupportEvent(const RgpTraceSourceSupportEvent& event) override;

        void RegisterStatusEvent(const TraceSourceStatusEvent& event) override;

        void RegisterTraceCompletionEvent(const TraceCompletionEvent& event) override;

        void RegisterTraceCaptureProgressEvent(const TraceCaptureProgressEvent& event) override;

        void PostSupportEvent(const RgpTraceSourceSupportEventArgs& args) const;

        void QueryStatus() override;

        Result PrepareForDelayedCapture(DDConnectionId connection_id) override;
        Result RequestBeginTrace(DDConnectionId connection_id, uint32_t capture_mode) override;
        void   GetSupportedCaptureModes(DDConnectionId connection_id, std::vector<uint32_t>& out_modes) override;

        RgpTraceSourceConfig& GetConfig() override;

        Result SetShaderInstrumentationEnabled(bool enabled) override;

        void Bind(RgpClient* client) override;

        /// @brief Processes SPM counters for the trace.
        /// @param [in] listener The listener object (self).
        /// @param [in] spm_trace The trace to process SPM counters for.
        static void OnTraceCompletedWithSpm(void* listener, const SpmTrace& spm_trace);

    private:
        const std::shared_ptr<SystemInfoCache> system_info_cache_;             ///< Object used to query the system info.
        std::shared_ptr<RgpSpmCounterHandler>  counter_handler_;               ///< Object used to handle SPM counters.
        DDDriverUtilsApi*                      driver_utils_api_;              ///< The driver utils API from devdriver registry.
        std::shared_ptr<Logger>                logger_;                        ///< Object used for debug logging.
        std::shared_ptr<RgpClientFactory>      client_factory_;                ///< The factory for the clients.
        BaseTriggerableTraceSource<RgpClient>  base_trace_source_;             ///< Base trace source.
        std::mutex                             support_event_mutex_;           ///< Mutex guards support event
        RgpTraceSourceSupportEvent             support_event_;                 ///< Support event callback;
        std::atomic_bool                       is_ubertrace_supported_;        ///< true if ubertrace protocol is supported.
        std::atomic_bool                       abort_spm_processing_ = false;  ///< true if SPM processing should be aborted, false otherwise.
        std::mutex                             shader_inst_mutex_;             ///< Mutex to guard shader instrumentation.
    };

}  // namespace devtrace

#endif
