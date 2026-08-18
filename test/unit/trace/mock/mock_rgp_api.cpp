//=============================================================================
// Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for mock RGP API.
//=============================================================================

#include "mock_rgp_api.h"

#include <stdexcept>

std::mutex          MockRgpApi::mutex_;
MockRgpApiDelegate* MockRgpApi::delegate_ = nullptr;

std::unique_lock<std::mutex> MockRgpApi::UseMock(DDGpuProfilingApi* api, MockRgpApiDelegate* delegate)
{
    if (api == nullptr || delegate == nullptr)
    {
        throw std::invalid_argument("api and delegate must both be non-null");
    }

    api->EnableTracing              = &MockRgpApi::EnableTracing;
    api->DisableTracing             = &MockRgpApi::DisableTracing;
    api->ExecuteTrace               = &MockRgpApi::ExecuteTrace;
    api->AbortTrace                 = &MockRgpApi::AbortTrace;
    api->SetSpmCounters             = &MockRgpApi::SetSpmCounters;
    api->QueryClientProtocolVersion = &MockRgpApi::QueryClientProtocolVersion;

    auto lock = std::unique_lock<std::mutex>(mutex_);
    delegate_ = delegate;

    return lock;
}

DD_RESULT MockRgpApi::EnableTracing(DDGpuProfilingInstance* instance, DDConnectionId umd_connection_id, const DDGpuProfilingConfig* config)
{
    return delegate_->EnableTracing(instance, umd_connection_id, config);
}

void MockRgpApi::DisableTracing(DDGpuProfilingInstance* instance, DDConnectionId umd_connection_id)
{
    delegate_->DisableTracing(instance, umd_connection_id);
}

DD_RESULT MockRgpApi::ExecuteTrace(DDGpuProfilingInstance* instance, DDConnectionId umd_connection_id, const DDGpuProfilingTraceArgs* args)
{
    return delegate_->ExecuteTrace(instance, umd_connection_id, args);
}

void MockRgpApi::AbortTrace(DDGpuProfilingInstance* instance, DDConnectionId umd_connection_id)
{
    delegate_->AbortTrace(instance, umd_connection_id);
}

DD_RESULT MockRgpApi::SetSpmCounters(DDGpuProfilingInstance*           instance,
                                     DDConnectionId                    umd_connection_id,
                                     const DDGpuProfilingSpmCounterId* counters,
                                     uint32_t                          num_counters)
{
    return delegate_->SetSpmCounters(instance, umd_connection_id, counters, num_counters);
}

DD_RESULT MockRgpApi::QueryClientProtocolVersion(DDGpuProfilingInstance* instance, DDConnectionId umd_connection_id, uint16_t* version)
{
    return delegate_->QueryClientProtocolVersion(instance, umd_connection_id, version);
}
