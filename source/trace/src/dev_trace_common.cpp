// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for global declares for LibDevTrace.

#include "dev_trace_common.h"

#include "dipper.h"

#include "chunk_writing.h"
#include "client_connection_manager.h"
#include "counters/rgp_spm_counter_handler.h"
#include "device_clocks.h"
#include "overlay_manager.h"
#include "system_info_cache.h"
#include "ubertrace_factory.h"

#include "rgd/rgd_trace_source_impl.h"
#include "rgp/rgp_trace_source_impl.h"
#include "rmv/rmv_trace_source_impl.h"
#include "rra/rra_trace_source_impl.h"

namespace devtrace
{
    bool IsComputeApi(const Api api)
    {
        return api == Api::kOpenCl || api == Api::kHip;
    }

    std::string GetHumanReadableName(const Api api)
    {
        switch (api)
        {
        case Api::kDirectX12:
            return "DirectX 12";
        case Api::kOpenCl:
            return "OpenCL";
        case Api::kHip:
            return "HIP";
        case Api::kVulkan:
            return "Vulkan";
        case Api::kOpenGl:
            return "OpenGL";
        case Api::kDirectX9:
            return "DirectX 9";
        case Api::kDirectX11:
            return "DirectX 11";
        default:
            break;
        }

        return "Unknown";
    }

    dipper::Container GetDefaultDipperContainer()
    {
        dipper::Container container;

        container.RegisterSingleton<SystemInfoCache, SystemInfoCache>();
        container.RegisterSingleton<AdditionalChunkWriter, AdditionalChunkWriter>();
        container.RegisterSingleton<OverlayManager, OverlayManager>();
        container.RegisterSingleton<DeviceClocksManager, DeviceClocksManager>();
        container.RegisterSingleton<UbertraceUserFactory, UbertraceUserFactory>();

        container.Register<ClientConnectionManager, ClientConnectionManager>();
        container.Register<ClientConnectionManagerV2, ClientConnectionManagerV2>();
        container.Register<RgpSpmCounterHandler, GpaSpmCounterHandler>();

        container.Register<RmvTraceSource, RmvTraceSourceImpl>();
        container.Register<RgpTraceSource, RgpTraceSourceImpl>();
        container.Register<RraTraceSource, RraTraceSourceImpl>();
        container.Register<RgdTraceSource, RgdTraceSourceImpl>();

        return container;
    }

};  // namespace devtrace
