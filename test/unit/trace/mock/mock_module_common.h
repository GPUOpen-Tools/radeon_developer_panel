//=============================================================================
// Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for mock DevDriver module common API.
//=============================================================================

#ifndef RDP_TEST_UNIT_TRACE_MOCK_MOCK_MODULE_COMMON_H_
#define RDP_TEST_UNIT_TRACE_MOCK_MOCK_MODULE_COMMON_H_

#include <mutex>

#include <gmock/gmock.h>

#include <ddApi.h>
#include <ddModule.h>

/// @brief The object for the mock common API to delegate to.
class MockCommonApiDelegate
{
public:
    MOCK_METHOD(DD_RESULT, QuerySystemInfo, (DDModuleSystemContext, void*, PFN_ddReceiveText));
    MOCK_METHOD(DD_RESULT, QueryStatus, (DDModuleClientContext));
    MOCK_METHOD(DD_RESULT, QueryClientProtocolVersion, (DDModuleClientContext, DDApiVersion*));
    MOCK_METHOD(DD_RESULT, QueryUserdataNode, (DDModuleDataContext, const char*, void*, PFN_ddReceiveBinary));
    MOCK_METHOD(DD_RESULT, UpdateUserdataNode, (DDModuleDataContext, const char*, const void*, size_t));
};

/// @brief A mock implementation of the DevDriver module common API.
///
/// Unfortunately because DevDriver uses raw function pointers, static functions need to be used.
/// In order to maintain thread-safety, only one test is allowed to be using the mock at any given time.
class MockCommonApi
{
public:
    /// @brief Reserves the use of the mock and populates api with pointers to the mock.
    ///
    /// Using the mock after the reservation has been released is undefined behavior.
    /// @param api The api to populate with the mock methods.
    /// @param delegate The delegate to use as the implementation of the API.
    /// @return A lock to hold until the mock is no longer needed.
    static std::unique_lock<std::mutex> UseMock(DDModuleCommonApi* api, MockCommonApiDelegate* delegate);

private:
    static DD_RESULT QuerySystemInfo(DDModuleSystemContext system_context, void* userdata, PFN_ddReceiveText receive_json);
    static DD_RESULT QueryStatus(DDModuleClientContext client_context);
    static DD_RESULT QueryClientProtocolVersion(DDModuleClientContext client_context, DDApiVersion* version);
    static DD_RESULT QueryUserdataNode(DDModuleDataContext data_context, const char* node_name, void* userdata, PFN_ddReceiveBinary receive_bytes);
    static DD_RESULT UpdateUserdataNode(DDModuleDataContext data_context, const char* node_name, const void* bytes, size_t bytes_size);

    static std::mutex             mutex_;     ///< The mutex used to reserve use of the mock.
    static MockCommonApiDelegate* delegate_;  ///< The delegate to use as the implementation of the API.
};

#endif
