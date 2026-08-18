// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for capture param handling.

#ifndef RDP_SOURCE_API_CAPTURE_PARAMS_H_
#define RDP_SOURCE_API_CAPTURE_PARAMS_H_

#include <memory>

#include "RdpCaptureApi.h"

namespace devtrace
{
    class RgpTraceSource;
    class RraTraceSource;
}  // namespace devtrace

/// @brief Gets the default profiling parameters.
/// @param [out] params The default profiling parameters.
void GetDefaultProfilingParams(RdpCaptureProfilingParams* params);

/// @brief Applies the profiling params to the trace source.
/// @param [in] params The params to apply to the trace source.
/// @param [in] source The trace source to apply the params to.
void ApplyProfilingParams(const RdpCaptureProfilingParams* params, const std::shared_ptr<devtrace::RgpTraceSource>& source);

/// @brief Gets the default raytracing parameters.
/// @param [out] params The default raytracing parameters.
void GetDefaultRaytracingParams(RdpCaptureRaytracingParams* params);

/// @brief Applies the raytracing params to the trace source.
/// @param [in] params The params to apply to the trace source.
/// @param [in] source The trace source to apply the params to.
void ApplyRaytracingParams(const RdpCaptureRaytracingParams* params, const std::shared_ptr<devtrace::RraTraceSource>& source);

#endif
