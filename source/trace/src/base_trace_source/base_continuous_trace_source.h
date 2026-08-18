// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for base continuous trace source.

#ifndef RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_BASE_CONTINUOUS_TRACE_SOURCE_V2_H_
#define RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_BASE_CONTINUOUS_TRACE_SOURCE_V2_H_

#include <memory>
#include <type_traits>

#include "base_trace_source.h"
#include "dev_trace_common.h"

namespace devtrace
{
    /// @brief A concept that checks whether a client type is continuous.
    template <typename ClientType>
    concept Continuous = std::is_base_of_v<ContinuousClient<typename ClientType::ClientConfigType>, ClientType>;

    /// @brief The base for a continuous trace source.
    /// @tparam [in] ClientType The client type for the trace source.
    template <Continuous ClientType>
    class BaseContinuousTraceSource final : public BaseTraceSource<ClientType>
    {
    public:
        /// @brief Constructor.
        /// @param [in] client_factory The factory that can be used to create clients.
        /// @param [in] stream_provider An object that provides streams.
        /// @param [in] overlay_manager Manager for developer mode overlay.
        /// @param [in] overlay_feature The feature of the overlay to use.
        /// @param [in] logger Object used for logging.
        BaseContinuousTraceSource(const std::shared_ptr<typename BaseTraceSource<ClientType>::ClientFactoryType>& client_factory,
                                  const std::shared_ptr<ReadWriteStreamProvider>&                                 stream_provider,
                                  const std::shared_ptr<OverlayManager>&                                          overlay_manager,
                                  OverlayFeature                                                                  overlay_feature,
                                  const std::shared_ptr<Logger>&                                                  logger);

        /// @brief Requests a dump be taken from the specified client.
        /// @param [in] client_id The id of the client from the trace source status.
        /// @return The result of requesting the dump.
        Result RequestDump(uint16_t client_id);

        /// @brief Adds a marker to the current trace for the specified client.
        /// @param [in] client_id The id of the client from the trace source status.
        /// @param [in] marker The marker to add.
        /// @return The result of adding the marker.
        Result AddMarker(uint16_t client_id, const std::string& marker);
    };

}  // namespace devtrace

#include "base_continuous_trace_source.inl"

#endif
