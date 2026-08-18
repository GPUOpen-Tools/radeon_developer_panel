// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for base triggerable trace source.

#ifndef RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_BASE_TRIGGERABLE_TRACE_SOURCE_V2_H_
#define RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_BASE_TRIGGERABLE_TRACE_SOURCE_V2_H_

#include <memory>
#include <type_traits>

#include "base_trace_source.h"
#include "dev_trace_common.h"

namespace devtrace
{
    /// @brief A concept that checks whether a client type is triggerable.
    template <typename ClientType>
    concept Triggerable = std::is_base_of_v<TriggerableClient<typename ClientType::ClientConfigType>, ClientType>;

    /// @brief The base for a triggerable trace source.
    /// @tparam [in] ClientType The client type for the trace source.
    template <Triggerable ClientType>
    class BaseTriggerableTraceSource final : public BaseTraceSource<ClientType>
    {
    public:
        /// @brief Constructor.
        /// @param [in] client_factory The factory that can be used to create clients.
        /// @param [in] stream_provider An object that provides streams.
        /// @param [in] overlay_manager Manager for developer mode overlay.
        /// @param [in] overlay_feature The feature of the overlay to use.
        /// @param [in] logger Object used for logging.
        /// @param [in] binding_delegate An optional delegate that will be invoked when a new client connects to bind to a client.
        BaseTriggerableTraceSource(const std::shared_ptr<typename BaseTraceSource<ClientType>::ClientFactoryType>& client_factory,
                                   const std::shared_ptr<ReadWriteStreamProvider>&                                 stream_provider,
                                   const std::shared_ptr<OverlayManager>&                                          overlay_manager,
                                   OverlayFeature                                                                  overlay_feature,
                                   const std::shared_ptr<Logger>&                                                  logger,
                                   IClientEventBindingDelegate<ClientType>*                                        binding_delegate = nullptr);

        /// @brief This can be called to move the connection into the waiting for capture state.
        ///
        /// After the trace source is in this state, calling RequestBeginTrace will actually begin a capture.
        /// @param [in] umd_connection_id The identifier of the connection to capture.
        /// @return kSuccess if the connection was successfully prepared for capture.
        Result PrepareForDelayedCapture(DDConnectionId umd_connection_id);

        /// @brief Requests that a trace be taken.
        /// @param [in] umd_connection_id The identifier of the connection to capture.
        /// @param [in] capture_mode The meaning of this capture mode is up to interpretation of the implementing client, but 0 (default) should always be valid.
        /// @return kSuccess if the trace was successfully requested.
        Result RequestBeginTrace(DDConnectionId umd_connection_id, uint32_t capture_mode);

        /// @brief Gets the supported capture modes for the specific connection.
        /// @param [in] umd_connection_id The identifier of the connection to get the supported modes for.
        /// @param [in] candidates The possible modes to check against the client for support.
        /// @param [out] out_modes The capture modes for the client.
        void GetSupportedCaptureModes(DDConnectionId umd_connection_id, const std::vector<uint32_t>& candidates, std::vector<uint32_t>& out_modes);
    };

}  // namespace devtrace

#include "base_triggerable_trace_source.inl"

#endif
