//=============================================================================
// Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for mock RGD native API.
//=============================================================================

#ifndef RDP_TEST_UNIT_TRACE_MOCK_MOCK_RGD_API_H_
#define RDP_TEST_UNIT_TRACE_MOCK_MOCK_RGD_API_H_

#include <cstdint>
#include <mutex>

#include <gmock/gmock.h>

#include <ddApi.h>
#include <dd_gpu_detective_api.h>

/// @brief The object for the mock native API to delegate to.
class MockRgdApiDelegate
{
public:
    MOCK_METHOD(DD_RESULT, EnableTracing, (DDGpuDetectiveInstance*, DDConnectionId, DDProcessId));
    MOCK_METHOD(void, DisableTracing, (DDGpuDetectiveInstance*, DDConnectionId));
    MOCK_METHOD(DD_RESULT, EndTracing, (DDGpuDetectiveInstance*, DDConnectionId, bool, bool*));
    MOCK_METHOD(DD_RESULT, TransferTraceData, (DDGpuDetectiveInstance*, DDConnectionId, const DDRdfFileWriter*, const DDIOHeartbeat*));
};

/// @brief A mock implementation of the RGD native API.
///
/// Unfortunately because DevDriver uses raw function pointers, static functions need to be used.
/// In order to maintain thread-safety, only one test is allowed to be using the mock at any given time.
class MockRgdApi
{
public:
    /// @brief Reserves the use of the mock and populates api with pointers to the mock.
    ///
    /// Using the mock after the reservation has been released is undefined behavior.
    /// @param [in] api The api to populate with the mock methods.
    /// @param [in] delegate The delegate to use as the implementation of the API.
    /// @return A lock to hold until the mock is no longer needed.
    static std::unique_lock<std::mutex> UseMock(DDGpuDetectiveApi* api, MockRgdApiDelegate* delegate);

private:
    static DD_RESULT EnableTracing(DDGpuDetectiveInstance* pInstance, DDConnectionId umdConnectionId, DDProcessId processId);
    static void      DisableTracing(DDGpuDetectiveInstance* pInstance, DDConnectionId umdConnectionId);
    static DD_RESULT EndTracing(DDGpuDetectiveInstance* pInstance, DDConnectionId umdConnectionId, bool isClientInitialized, bool* didDetectCrash);
    static DD_RESULT TransferTraceData(DDGpuDetectiveInstance*       pInstance,
                                       DDConnectionId                umdConnectionId,
                                       const struct DDRdfFileWriter* pRdfFileWriter,
                                       const struct DDIOHeartbeat*   pHeartbeat);

    static std::mutex          mutex_;     ///< The mutex used to reserve use of the mock.
    static MockRgdApiDelegate* delegate_;  ///< The delegate to use as the implementation of the API.
};

#endif
