// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for available UberTrace features.

#include "ubertrace_features.h"

#include <system_info_reader.h>

#include "dev_trace_common.h"

static constexpr uint32_t kMinimumNewReturnCodeDriverMajorVersion = 24;
static constexpr uint32_t kMinimumNewReturnCodeDriverMinorVersion = 10;

static constexpr uint32_t kMinimumNewControllerDriverMajorVersion = 24;
static constexpr uint32_t kMinimumNewControllerDriverMinorVersion = 20;

static constexpr uint32_t kMinimumGraphicsRenderOpDriverMajorVersion = 24;
static constexpr uint32_t kMinimumGraphicsRenderOpDriverMinorVersion = 30;

static constexpr uint32_t kMinimumCancelTraceMajorVersion = 24;
static constexpr uint32_t kMinimumCancelTraceMinorVersion = 30;

namespace devtrace
{
    UbertraceFeatures::UbertraceFeatures(const system_info_utils::SystemInfo& sys_info)
    {
        use_new_not_ready_code_    = !IsDriverTooOld(sys_info.driver, kMinimumNewReturnCodeDriverMajorVersion, kMinimumNewReturnCodeDriverMinorVersion);
        use_new_controller_format_ = !IsDriverTooOld(sys_info.driver, kMinimumNewControllerDriverMajorVersion, kMinimumNewControllerDriverMinorVersion);
        render_op_supported_graphics_ =
            !IsDriverTooOld(sys_info.driver, kMinimumGraphicsRenderOpDriverMajorVersion, kMinimumGraphicsRenderOpDriverMinorVersion);

        cancel_trace_supported_ = !IsDriverTooOld(sys_info.driver, kMinimumCancelTraceMajorVersion, kMinimumCancelTraceMinorVersion) ||
                                  getenv("RDP_ENABLE_UBERTRACE_CANCEL") != nullptr;
    }

    DD_RESULT UbertraceFeatures::GetTraceReadyCode() const
    {
        return use_new_not_ready_code_ ? DD_RESULT_DD_GENERIC_NOT_READY : DD_RESULT_COMMON_UNSUPPORTED;
    }

    bool UbertraceFeatures::UseNewControllerFormat() const
    {
        return use_new_controller_format_;
    }

    bool UbertraceFeatures::RenderOpControllerSupported() const
    {
        return render_op_supported_graphics_;
    }

    bool UbertraceFeatures::IsCancelTraceSupported() const
    {
        return cancel_trace_supported_;
    }

    UbertraceController UbertraceFeatures::GetFrameController(bool               enabled,
                                                              uint32_t           num_prep_frames,
                                                              uint32_t           num_capture_frames,
                                                              const std::string& capture_mode,
                                                              uint32_t           prep_start_index) const
    {
        UbertraceController controller{};
        controller.name   = UseNewControllerFormat() ? "frame" : "framecontroller";
        controller.config = std::make_shared<UbertraceFrameControllerConfig>(enabled, num_prep_frames, num_capture_frames, capture_mode, prep_start_index);

        return controller;
    }
}  // namespace devtrace
