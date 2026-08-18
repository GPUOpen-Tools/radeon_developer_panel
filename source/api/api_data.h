// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for utils to convert devtrace data to RDP capture api data.

#ifndef SOURCE_API_CAPTURE_API_DATA_H_
#define SOURCE_API_CAPTURE_API_DATA_H_

#include <dev_trace_common.h>
#include <source_status.h>

#include "RdpCaptureApi.h"

/// @brief Converts the devtrace API to a capture API.
/// @param [in] api the devtrace API.
/// @return The capture API.
inline RdpCaptureGpuApi GetGpuApi(devtrace::Api api)
{
    switch (api)
    {
    case devtrace::Api::kDirectX12:
        return kRdpCaptureGpuApiDirectX12;
    case devtrace::Api::kDirectX11:
        return kRdpCaptureGpuApiDirectX11;
    case devtrace::Api::kDirectX9:
        return kRdpCaptureGpuApiDirectX9;
    case devtrace::Api::kVulkan:
        return kRdpCaptureGpuApiVulkan;
    case devtrace::Api::kOpenCl:
        return kRdpCaptureGpuApiOpenCl;
    case devtrace::Api::kHip:
        return kRdpCaptureGpuApiHip;
    case devtrace::Api::kOpenGl:
        return kRdpCaptureGpuApiOpenGl;
    default:
        return kRdpCaptureGpuApiUnknown;
    }
}

/// @brief Converts the devtrace result to a capture result.
/// @param [in] result the devtrace result.
/// @return The capture result.
inline RdpCaptureResult GetCaptureResult(devtrace::Result result)
{
    switch (result)
    {
    case devtrace::Result::kSuccess:
        return kRdpCaptureResultSuccess;
    case devtrace::Result::kFailure:
        return kRdpCaptureResultFailure;
    case devtrace::Result::kUnsupported:
        return kRdpCaptureResultUnsupported;
    case devtrace::Result::kNotFound:
    case devtrace::Result::kExecutableNotFound:
        return kRdpCaptureResultNotFound;
    case devtrace::Result::kNotReady:
        return kRdpCaptureResultNotReady;
    default:
        return kRdpCaptureResultUnknown;
    }
}

/// @brief Converts the devtrace trace completion status to a capture result.
/// @param [in] status the trace completion status.
/// @return The capture result.
inline RdpCaptureResult GetCaptureResultForTraceCompletionStatus(devtrace::TraceCompletionStatus status)
{
    switch (status)
    {
    case devtrace::TraceCompletionStatus::kCompleted:
        return kRdpCaptureResultSuccess;
    case devtrace::TraceCompletionStatus::kError:
        return kRdpCaptureResultFailure;
    case devtrace::TraceCompletionStatus::kAborted:
        return kRdpCaptureResultAborted;
    default:
        return kRdpCaptureResultUnknown;
    }
}

/// @brief Gets the capture feature stage for the disabled reason.
/// @param [in] reason The reason to get the stage for.
/// @return The stage based on the reason.
inline RdpCaptureFeatureStage GetDisabledStage(devtrace::DisabledReason reason)
{
    switch (reason)
    {
    case devtrace::DisabledReason::kEnabled:
        DEV_TRACE_ASSERT(false);
        return kRdpCaptureFeatureStageDisabled;
    case devtrace::DisabledReason::kEncounteredError:
        return kRdpCaptureFeatureStageDisabledFromError;
    case devtrace::DisabledReason::kHardwareUnsupported:
        return kRdpCaptureFeatureStageHardwareUnsupported;
    case devtrace::DisabledReason::kOsUnsupported:
        return kRdpCaptureFeatureStageOsUnsupported;
    case devtrace::DisabledReason::kDriverUnsupported:
        return kRdpCaptureFeatureStageDriverUnsupported;
    case devtrace::DisabledReason::kApiUnsupported:
        return kRdpCaptureFeatureStageApiUnsupported;
    default:
        return kRdpCaptureFeatureStageDisabled;
    }
}

/// @brief Converts the devtrace trace source status to a capture feature stage.
/// @param [in] status the trace source status.
/// @return The capture feature stage.
inline RdpCaptureFeatureStage GetCaptureStage(const devtrace::TraceSourceStatus& status)
{
    switch (status.GetStage())
    {
    case devtrace::TraceSourceStage::kDisconnected:
        return kRdpCaptureFeatureStageDisconnected;
    case devtrace::TraceSourceStage::kIdle:
        return kRdpCaptureFeatureStageReadyForCapture;
    case devtrace::TraceSourceStage::kWaitingToBeginCapture:
        return kRdpCaptureFeatureStageWaitingToBeginCapture;
    case devtrace::TraceSourceStage::kCapturing:
    case devtrace::TraceSourceStage::kDumping:
    case devtrace::TraceSourceStage::kProcessing:
        return kRdpCaptureFeatureStageCapturing;
    case devtrace::TraceSourceStage::kDone:
        return kRdpCaptureFeatureStageDone;
    case devtrace::TraceSourceStage::kDisabled:
        return GetDisabledStage(status.disabled_reason);
    case devtrace::TraceSourceStage::kError:
        return kRdpCaptureFeatureStageEncounteredError;
    case devtrace::TraceSourceStage::kBusy:
        return kRdpCaptureFeatureStageBusy;
    default:
        return kRdpCaptureFeatureStageUnknown;
    }
}

/// @brief Converts the devtrace trace source status to a detailed capture stage.
/// @param [in] status the trace source status.
/// @return The detailed capture stage.
inline RdpCaptureDetailedStage GetDetailedCaptureStage(const devtrace::TraceSourceStatus& status)
{
    switch (status.GetStage())
    {
    case devtrace::TraceSourceStage::kDisconnected:
        return kRdpCaptureDetailedStageDisconnected;
    case devtrace::TraceSourceStage::kIdle:
        return kRdpCaptureDetailedStageReadyForCapture;
    case devtrace::TraceSourceStage::kWaitingToBeginCapture:
        return kRdpCaptureDetailedStageWaitingToBeginCapture;
    case devtrace::TraceSourceStage::kCapturing:
        return kRdpCaptureDetailedStageCapturing;
    case devtrace::TraceSourceStage::kDumping:
        return kRdpCaptureDetailedStageDumping;
    case devtrace::TraceSourceStage::kProcessing:
        return kRdpCaptureDetailedStageProcessing;
    case devtrace::TraceSourceStage::kDone:
        return kRdpCaptureDetailedStageDone;
    case devtrace::TraceSourceStage::kDisabled:
    {
        switch (status.disabled_reason)
        {
        case devtrace::DisabledReason::kEncounteredError:
            return kRdpCaptureDetailedStageDisabledFromError;
        case devtrace::DisabledReason::kHardwareUnsupported:
            return kRdpCaptureDetailedStageHardwareUnsupported;
        case devtrace::DisabledReason::kOsUnsupported:
            return kRdpCaptureDetailedStageOsUnsupported;
        case devtrace::DisabledReason::kDriverUnsupported:
            return kRdpCaptureDetailedStageDriverUnsupported;
        case devtrace::DisabledReason::kApiUnsupported:
            return kRdpCaptureDetailedStageApiUnsupported;
        default:
            return kRdpCaptureDetailedStageDisabled;
        }
    }
    case devtrace::TraceSourceStage::kError:
        return kRdpCaptureDetailedStageEncounteredError;
    case devtrace::TraceSourceStage::kBusy:
        return kRdpCaptureDetailedStageBusy;
    default:
        return kRdpCaptureDetailedStageUnknown;
    }
}

#endif
