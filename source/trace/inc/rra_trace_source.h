// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for base RRA trace source.

#ifndef RDP_SOURCE_TRACE_INC_RRA_TRACE_SOURCE_H_
#define RDP_SOURCE_TRACE_INC_RRA_TRACE_SOURCE_H_

#include <array>
#include <atomic>

#include "configurable.h"
#include "triggerable_trace_source.h"

namespace devtrace
{
    struct RraTraceSourceSupportEventArgs
    {
        bool is_ray_history_supported    = true;   ///< true if ray history is supported.
        bool is_marker_capture_supported = false;  ///< true if marker-based capture is supported (requires driver >= 26.20).
    };

    struct RraTraceSourceSupportEvent
    {
        void*                                                             listener;
        std::function<void(void*, const RraTraceSourceSupportEventArgs&)> callback;
    };

    /// @brief Configuration for RRA trace source.
    struct RraTraceSourceConfig
    {
        /// @brief Ray history buffer size default options. (Minimum 250MB, Low 600MB, Default 1GB, High 2GB, Maximum 4GB)
        /// This is an initial estimate which could evolve after testing.
        static constexpr std::array kRayHistoryBufferSizes = {"268435456", "629145600", "1073741824", "2684354560", "4000000000"};

        std::atomic<uint64_t> ray_history_buffer_size = 0;     ///< The size of the ray history buffer in bytes.
        std::atomic_bool      enable_ray_history      = true;  ///< true if ray history source should be enabled.

        std::atomic_bool enable_marker_capture       = false;             ///< true if marker-based capture should be used.
        std::atomic_bool is_marker_capture_supported = false;             ///< true if marker-based capture is supported by the driver.
        std::string      marker_begin_string         = "RRABeginMarker";  ///< The marker string that starts the capture.
        std::string      marker_end_string           = "RRAEndMarker";    ///< The marker string that ends the capture.
    };

    /// @brief Abstract RRA trace source.
    class RraTraceSource : public TriggerableTraceSource, public Configurable<RraTraceSourceConfig>
    {
    public:
        /// @brief Destructor.
        ~RraTraceSource() override = default;

        /// @brief Registers a support event listener.
        /// @param [in] event The event to register.
        virtual void RegisterSupportEvent(const RraTraceSourceSupportEvent& event) = 0;
    };

};  // namespace devtrace

#endif
