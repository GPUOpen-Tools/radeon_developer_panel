// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the main crash analysis client.

#ifndef RDP_SOURCE_TRACE_SRC_RGD_CLIENT_H_
#define RDP_SOURCE_TRACE_SRC_RGD_CLIENT_H_

#include <memory>
#include <string>

#include <ddApi.h>
#include <dd_enhanced_crash_info_api.h>
#include <dd_gpu_detective_api.h>

#include "../base_trace_source/trace_source_client.h"
#include "chunk_writing.h"

#include "dev_trace_common.h"
#include "rgd_chunk_writer.h"
#include "rgd_trace_source.h"
#include "rgd_ubertrace_client.h"

namespace devtrace
{
    /// @brief RGD Trace source client.
    class RgdClient final : public ContinuousClient<RgdTraceSourceConfig>
    {
    public:
        /// @brief Constructor.
        /// @param [in] conn_info The connection information for the client.
        /// @param [in] client_utils The utils that the client can use.
        /// @param [in] gpu_detective_api The RGD API.
        /// @param [in] enhanced_api The enhanced crash info API.
        /// @param [in] ubertrace_client The client to use to augment this client with UberTrace. A null client will disable UberTrace.
        /// @param [in] additional_chunk_writer The object that does additional chunk writing.
        RgdClient(ClientConnection                              conn_info,
                  ClientUtils<RgdTraceSourceConfig>&            client_utils,
                  DDGpuDetectiveApi*                            gpu_detective_api,
                  DDEnhancedCrashInfoApi*                       enhanced_api,
                  std::unique_ptr<RgdUbertraceClient>           ubertrace_client,
                  const std::shared_ptr<AdditionalChunkWriter>& additional_chunk_writer);

        /// @brief Destructor.
        ~RgdClient() override;

        /// @brief Callback for writing status events.
        /// @param [in] object The object the event is for.
        /// @param [in] args The event arguments.
        static void OnWritingStatusEvent(void* object, const WritingStatusEventArgs& args);

        Result HandleDriverState(DD_DRIVER_STATE state) override;

        Result RequestAbortTrace() override;

        bool IsAbortTraceSupported() override;

        Result RequestDump() override;

        Result AddMarker(const std::string& marker) override;

        DD_DRIVER_STATE GetInitDriverState() override;
        Result          Initialize() override;

        void Disconnect() override;

    private:
        /// @brief Enables hardware crash analysis if the config specifies that it should be.
        /// @return The result of enabling hardware crash analysis or DD_RESULT_SUCCESS if it was not enabled.
        DD_RESULT HandleHardwareCrashAnalysis();

        /// @brief Returns an observable that will dump the crash.
        /// @return An observable that will dump the crash.
        void DumpCrash();

        ClientConnection                       conn_info_{};                           ///< The connection info for the client.
        DDGpuDetectiveApi*                     gpu_detective_api_ = nullptr;           ///< The RGD API.
        DDEnhancedCrashInfoApi*                enhanced_api_      = nullptr;           ///< The enhanced crash info API.
        std::unique_ptr<RgdUbertraceClient>    ubertrace_client_;                      ///< The client to use to augment this client with UberTrace.
        std::shared_ptr<AdditionalChunkWriter> additional_chunk_writer_;               ///< The object that does additional chunk writing.
        ClientUtils<RgdTraceSourceConfig>&     client_utils_;                          ///< The utils that the client can use.
        bool                                   has_reached_post_device_init_ = false;  ///< true if the client reached post device init, false otherwise.
        ChunkWriterReservationId               chunk_reservation_            = kInvalidChunkWriterReservationId;  ///< Reservation for chunk writer.
        ProcessInfoChunk                       process_info_chunk_;                                               ///< The process info chunk.
        ExtendedInfoChunkWriter                extended_info_chunk_;                                              ///< The extended info chunk writer.
    };
}  // namespace devtrace

#endif
