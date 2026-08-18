//=============================================================================
// Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for mock RRA native API.
//=============================================================================

#include "mock_rra_api.h"

#include <stdexcept>

std::mutex          MockRraApi::mutex_;
MockRraApiDelegate* MockRraApi::delegate_ = nullptr;

std::unique_lock<std::mutex> MockRraApi::UseMock(DDUberTraceApi* api, MockRraApiDelegate* delegate)
{
    if (api == nullptr || delegate == nullptr)
    {
        throw std::invalid_argument("api and delegate must both be non-null");
    }

    api->Connect              = &MockRraApi::Connect;
    api->Disconnect           = &MockRraApi::Disconnect;
    api->EnableTracing        = &MockRraApi::EnableTracing;
    api->ConfigureTraceParams = &MockRraApi::ConfigureTraceParams;
    api->RequestTrace         = &MockRraApi::RequestTrace;
    api->CancelTrace          = &MockRraApi::CancelTrace;
    api->CollectTrace         = &MockRraApi::CollectTrace;

    auto lock = std::unique_lock<std::mutex>(mutex_);
    delegate_ = delegate;

    return lock;
}

DD_RESULT MockRraApi::Connect(DDUberTraceInstance* instance, DDConnectionId umd_connection_id)
{
    return delegate_->Connect(instance, umd_connection_id);
}

void MockRraApi::Disconnect(DDUberTraceInstance* instance, DDConnectionId umd_connection_id)
{
    delegate_->Disconnect(instance, umd_connection_id);
}

DD_RESULT MockRraApi::EnableTracing(DDUberTraceInstance* instance, DDConnectionId umd_connection_id)
{
    return delegate_->EnableTracing(instance, umd_connection_id);
}

DD_RESULT MockRraApi::ConfigureTraceParams(DDUberTraceInstance* instance, DDConnectionId umd_connection_id, const char* data, size_t data_size)
{
    return delegate_->ConfigureTraceParams(instance, umd_connection_id, data, data_size);
}

DD_RESULT MockRraApi::RequestTrace(DDUberTraceInstance* instance, DDConnectionId umd_connection_id)
{
    return delegate_->RequestTrace(instance, umd_connection_id);
}

DD_RESULT MockRraApi::CancelTrace(DDUberTraceInstance* instance, DDConnectionId umd_connection_id)
{
    return delegate_->CancelTrace(instance, umd_connection_id);
}

DD_RESULT MockRraApi::CollectTrace(DDUberTraceInstance* instance, DDConnectionId umd_connection_id, uint32_t timeout_ms, const DDByteWriter* writer)
{
    return delegate_->CollectTrace(instance, umd_connection_id, timeout_ms, writer);
}
