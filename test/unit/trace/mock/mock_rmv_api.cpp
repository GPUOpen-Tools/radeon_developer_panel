//=============================================================================
// Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for mock RMV API.
//=============================================================================

#include "mock_rmv_api.h"

#include <stdexcept>

std::mutex          MockRmvApi::mutex_;
MockRmvApiDelegate* MockRmvApi::delegate_ = nullptr;

std::unique_lock<std::mutex> MockRmvApi::UseMock(DDMemoryTraceApi* api, MockRmvApiDelegate* delegate)
{
    if (api == nullptr || delegate == nullptr)
    {
        throw std::invalid_argument("api and delegate must both be non-null");
    }

    api->EnableTracing     = &MockRmvApi::EnableTracing;
    api->DisableTracing    = &MockRmvApi::DisableTracing;
    api->EndTracing        = &MockRmvApi::EndTracing;
    api->DumpTrace         = &MockRmvApi::DumpTrace;
    api->AbortTrace        = &MockRmvApi::AbortTrace;
    api->InsertSnapshot    = &MockRmvApi::InsertSnapshot;
    api->ClearTrace        = &MockRmvApi::ClearTrace;
    api->QueryStatus       = &MockRmvApi::QueryStatus;
    api->TransferTraceData = &MockRmvApi::TransferTraceData;

    auto lock = std::unique_lock<std::mutex>(mutex_);
    delegate_ = delegate;

    return lock;
}

DD_RESULT MockRmvApi::EnableTracing(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id, DDProcessId process_id, bool use_kmd)
{
    return delegate_->EnableTracing(instance, umd_connection_id, process_id, use_kmd);
}

DD_RESULT MockRmvApi::DisableTracing(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id)
{
    return delegate_->DisableTracing(instance, umd_connection_id);
}

DD_RESULT MockRmvApi::EndTracing(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id, bool is_client_initialized)
{
    return delegate_->EndTracing(instance, umd_connection_id, is_client_initialized);
}

DD_RESULT MockRmvApi::DumpTrace(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id, bool is_client_initialized)
{
    return delegate_->DumpTrace(instance, umd_connection_id, is_client_initialized);
}

DD_RESULT MockRmvApi::AbortTrace(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id, bool is_client_initialized)
{
    return delegate_->AbortTrace(instance, umd_connection_id, is_client_initialized);
}

DD_RESULT MockRmvApi::InsertSnapshot(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id, const char* snapshot_name)
{
    return delegate_->InsertSnapshot(instance, umd_connection_id, snapshot_name);
}

DD_RESULT MockRmvApi::ClearTrace(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id)
{
    return delegate_->ClearTrace(instance, umd_connection_id);
}

DD_RESULT MockRmvApi::QueryStatus(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id, DDMemoryTraceStatus* status)
{
    return delegate_->QueryStatus(instance, umd_connection_id, status);
}

DD_RESULT MockRmvApi::TransferTraceData(DDMemoryTraceInstance* instance,
                                        DDConnectionId         umd_connection_id,
                                        const DDRdfFileWriter* file_writer,
                                        const DDIOHeartbeat*   heartbeat,
                                        bool                   compressed)
{
    return delegate_->TransferTraceData(instance, umd_connection_id, file_writer, heartbeat, compressed);
}
