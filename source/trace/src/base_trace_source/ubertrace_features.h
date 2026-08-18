// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for available UberTrace features.

#ifndef RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_UBERTRACE_FEATURES_H_
#define RDP_SOURCE_TRACE_SRC_BASE_TRACE_SOURCE_UBERTRACE_FEATURES_H_

#include <string>

#include <dd_common_api.h>

#include "ubertrace_params.h"

namespace system_info_utils
{
    struct SystemInfo;
}

namespace devtrace
{
    /// @brief Contains all the driver-side feature information for UberTrace.
    struct UbertraceFeatures
    {
        /// @brief Constructor.
        UbertraceFeatures() = default;

        /// @brief Constructor.
        /// @param [in] sys_info The system info to derive the features from.
        explicit UbertraceFeatures(const system_info_utils::SystemInfo& sys_info);

        /// @brief Gets the result code for the trace not being ready.
        /// @return The return code for the trace not being ready.
        [[nodiscard]] DD_RESULT GetTraceReadyCode() const;

        /// @brief Returns whether the new controller format should be used, false otherwise.
        /// @return true if the new controller format should be used, false otherwise.
        [[nodiscard]] bool UseNewControllerFormat() const;

        /// @brief Returns whether the render op trace controller is supported for graphics APIs.
        /// @return true if the render op trace controller is supported for graphics APIs.
        [[nodiscard]] bool RenderOpControllerSupported() const;

        /// @beif Returns whether the cancel trace operation is supported.
        /// @return true if the cancel trace operation is supported.
        [[nodiscard]] bool IsCancelTraceSupported() const;

        /// @brief Creates a frame controller
        /// @param [in] enabled true if the controller is enabled, false otherwise.
        /// @param [in] num_prep_frames The number of prep frames that the capture should have.
        /// @param [in] num_capture_frames The number of frames to capture.
        /// @param [in] capture_mode The capture mode.
        /// @param [in] prep_start_index The index of the start of preparation.
        UbertraceController GetFrameController(bool               enabled,
                                               uint32_t           num_prep_frames,
                                               uint32_t           num_capture_frames,
                                               const std::string& capture_mode,
                                               uint32_t           prep_start_index) const;

    private:
        bool use_new_not_ready_code_       = false;  ///< true if the new code for the trace not being ready should be used.
        bool use_new_controller_format_    = false;  ///< true if the new controller format should be used, false otherwise.
        bool render_op_supported_graphics_ = false;  ///< true if the renderop trace controller is supported for graphics apis.
        bool cancel_trace_supported_       = false;  ///< true if the cancel trace operation is supported.
    };

}  // namespace devtrace

#endif
