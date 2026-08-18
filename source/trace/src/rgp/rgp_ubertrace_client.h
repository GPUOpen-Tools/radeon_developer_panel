// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the Ubertrace RGP trace source client.

#ifndef RDP_SOURCE_TRACE_SRC_RGP_RGP_UBERTRACE_CLIENT_H_
#define RDP_SOURCE_TRACE_SRC_RGP_RGP_UBERTRACE_CLIENT_H_

#include <memory>

#include "../base_trace_source/ubertrace_client.h"

#include "active_gpu_provider.h"
#include "rgp_trace_client.h"

namespace devtrace
{
    /// @brief base Client used for RGP UberTrace.
    class RgpUbertraceClient final : public UbertraceClient<RgpTraceSourceConfigPrivate>, public ISpmTraceClient
    {
    public:
        /// @brief Constructor.
        /// @param [in] conn_info The connection information for the client.
        /// @param [in] client_utils The utils that the client can use.
        /// @param [in] user The UberTrace user.
        /// @param [in] addl_chunk_writer Object used for system info writing.
        /// @param [in] active_gpu_provider Active GPU provider.
        /// @param [in] counter_handler The handler for counters.
        /// @param [in] device_clocks_manager The device clocks manager.
        RgpUbertraceClient(const ClientConnection&                           conn_info,
                           ClientUtils<RgpTraceSourceConfigPrivate>&         client_utils,
                           std::unique_ptr<UbertraceUser>                    user,
                           const std::shared_ptr<AdditionalChunkWriter>&     addl_chunk_writer,
                           std::unique_ptr<ActiveGpuProvider>&               active_gpu_provider,
                           const std::shared_ptr<RgpSpmCounterHandler>&      counter_handler,
                           const std::shared_ptr<class DeviceClocksManager>& device_clocks_manager);

        void   Disconnect() override;
        Result HandleDriverState(DD_DRIVER_STATE state) override;

        void RegisterSpmTraceListener(const SpmTraceEvent& event) override;

    protected:
        std::optional<UbertraceAutoCaptureConfig> GetAutoCaptureConfig(const RgpTraceSourceConfigPrivate& config) override;
        void                                      GetEarlyConfiguration(const RgpTraceSourceConfigPrivate& config, UberTraceConfig& early_config) override;
        Result GenerateCaptureConfig(const RgpTraceSourceConfigPrivate& config, UbertraceCaptureConfig& capture_config) override;

    private:
        /// Gets the config for the GPU perf source.
        /// @param [in] config The client config.
        /// @param [in] gpu The target GPU for the capture.
        /// @param [out] gpu_perf_config The generated config.
        /// @param [out] on_trace_completed The function to call when the trace is completed.
        /// @return The result of generating the config.
        Result GetGpuPerfConfig(const RgpTraceSourceConfigPrivate&       config,
                                const system_info_utils::GpuInfo&        gpu,
                                std::shared_ptr<UbertraceSourceConfig>&  gpu_perf_config,
                                std::function<void(const std::string&)>& on_trace_completed);

        /// @brief Queries SPM counters.
        /// @param [in] config The client config.
        /// @param [in] gpu The target GPU for the capture.
        /// @param [out] counters The queried counters.
        /// @return The result of the query.
        Result QuerySpmCounters(const RgpTraceSourceConfigPrivate& config, const system_info_utils::GpuInfo& gpu, SpmCounterQueryResult& counters);

        /// @brief Generates the controller for the capture.
        /// @param [in] capture_mode The capture mode for the capture.
        /// @param [in] capture_type The mode of the capture being executed.
        /// @param [in] config The client config.
        /// @return The controller to use for the capture.
        [[nodiscard]] UbertraceController GenerateController(RgpCaptureMode                     capture_mode,
                                                             UberTraceCaptureType               capture_type,
                                                             const RgpTraceSourceConfigPrivate& config) const;

        /// @brief Callback handles updating the active GPU from polling
        /// @param [in] self A pointer to this client.
        /// @param [in] gpu The active GPU discovered through polling system info.
        static void OnActiveGpuUpdate(void* self, const system_info_utils::GpuInfo& gpu);

        void TracingStarted() override;
        void TracingEnded() override;

    public:
        [[nodiscard]] bool SupportsCaptureMode(uint32_t mode) const override;

    private:
        std::unique_ptr<ActiveGpuProvider>    active_gpu_provider_;    ///< Active GPU provider.
        std::shared_ptr<RgpSpmCounterHandler> counter_handler_;        ///< The handler for counters.
        std::shared_ptr<DeviceClocksManager>  device_clocks_manager_;  ///< The device clocks manager.
        system_info_utils::GpuInfo            active_gpu_{};           ///< The active GPU.
        SpmTraceEvent                         spm_trace_event_;        ///< The event to call when a trace with SPM is completed.
    };

}  // namespace devtrace

#endif
