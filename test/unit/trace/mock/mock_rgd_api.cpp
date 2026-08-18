//=============================================================================
// Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for mock RGD native API.
//=============================================================================

#include "mock_rgd_api.h"

#include <stdexcept>

std::mutex          MockRgdApi::mutex_;
MockRgdApiDelegate* MockRgdApi::delegate_ = nullptr;

std::unique_lock<std::mutex> MockRgdApi::UseMock(DDGpuDetectiveApi* api, MockRgdApiDelegate* delegate)
{
    if (api == nullptr || delegate == nullptr)
    {
        throw std::invalid_argument("api and delegate must both be non-null");
    }

    api->EnableTracing     = &MockRgdApi::EnableTracing;
    api->DisableTracing    = &MockRgdApi::DisableTracing;
    api->EndTracing        = &MockRgdApi::EndTracing;
    api->TransferTraceData = &MockRgdApi::TransferTraceData;

    auto lock = std::unique_lock<std::mutex>(mutex_);
    delegate_ = delegate;

    return lock;
}

DD_RESULT MockRgdApi::EnableTracing(DDGpuDetectiveInstance* pInstance, DDConnectionId umdConnectionId, DDProcessId processId)
{
    return delegate_->EnableTracing(pInstance, umdConnectionId, processId);
}

void MockRgdApi::DisableTracing(DDGpuDetectiveInstance* pInstance, DDConnectionId umdConnectionId)
{
    delegate_->DisableTracing(pInstance, umdConnectionId);
}

DD_RESULT MockRgdApi::EndTracing(DDGpuDetectiveInstance* pInstance, DDConnectionId umdConnectionId, bool isClientInitialized, bool* didDetectCrash)
{
    return delegate_->EndTracing(pInstance, umdConnectionId, isClientInitialized, didDetectCrash);
}

DD_RESULT MockRgdApi::TransferTraceData(DDGpuDetectiveInstance*       pInstance,
                                        DDConnectionId                umdConnectionId,
                                        const struct DDRdfFileWriter* pRdfFileWriter,
                                        const struct DDIOHeartbeat*   pHeartbeat)
{
    return delegate_->TransferTraceData(pInstance, umdConnectionId, pRdfFileWriter, pHeartbeat);
}
