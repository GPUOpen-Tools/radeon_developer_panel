//=============================================================================
// Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for mock DevDriver module common API.
//=============================================================================

#include "mock_module_common.h"

#include <stdexcept>

std::mutex             MockCommonApi::mutex_;
MockCommonApiDelegate* MockCommonApi::delegate_ = nullptr;

std::unique_lock<std::mutex> MockCommonApi::UseMock(DDModuleCommonApi* api, MockCommonApiDelegate* delegate)
{
    if (api == nullptr || delegate == nullptr)
    {
        throw std::invalid_argument("api and delegate must both be non-null");
    }

    auto lock = std::unique_lock<std::mutex>(mutex_);
    delegate_ = delegate;

    api->pfnQueryClientProtocolVersion = &MockCommonApi::QueryClientProtocolVersion;
    api->pfnQueryStatus                = &MockCommonApi::QueryStatus;
    api->pfnQuerySystemInfo            = &MockCommonApi::QuerySystemInfo;
    api->pfnQueryUserdataNode          = &MockCommonApi::QueryUserdataNode;
    api->pfnUpdateUserdataNode         = &MockCommonApi::UpdateUserdataNode;

    return lock;
}

DD_RESULT MockCommonApi::QuerySystemInfo(DDModuleSystemContext system_context, void* userdata, PFN_ddReceiveText receive_json)
{
    return delegate_->QuerySystemInfo(system_context, userdata, receive_json);
}

DD_RESULT MockCommonApi::QueryStatus(DDModuleClientContext client_context)
{
    return delegate_->QueryStatus(client_context);
}

DD_RESULT MockCommonApi::QueryClientProtocolVersion(DDModuleClientContext client_context, DDApiVersion* version)
{
    return delegate_->QueryClientProtocolVersion(client_context, version);
}

DD_RESULT MockCommonApi::QueryUserdataNode(DDModuleDataContext data_context, const char* node_name, void* userdata, PFN_ddReceiveBinary receive_bytes)
{
    return delegate_->QueryUserdataNode(data_context, node_name, userdata, receive_bytes);
}

DD_RESULT MockCommonApi::UpdateUserdataNode(DDModuleDataContext data_context, const char* node_name, const void* bytes, size_t bytes_size)
{
    return delegate_->UpdateUserdataNode(data_context, node_name, bytes, bytes_size);
}
