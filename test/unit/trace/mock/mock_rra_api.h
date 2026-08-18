// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for mock RRA native API.

#ifndef RDP_TEST_UNIT_TRACE_MOCK_MOCK_RRA_API_H_
#define RDP_TEST_UNIT_TRACE_MOCK_MOCK_RRA_API_H_

#include <cstdint>
#include <mutex>

#include <gmock/gmock.h>

#include <ddApi.h>
#include <dd_uber_trace_api.h>

/// @brief The object for the mock native API to delegate to.
class MockRraApiDelegate
{
public:
    MOCK_METHOD(DD_RESULT, Connect, (DDUberTraceInstance*, DDConnectionId));
    MOCK_METHOD(void, Disconnect, (DDUberTraceInstance*, DDConnectionId));
    MOCK_METHOD(DD_RESULT, EnableTracing, (DDUberTraceInstance*, DDConnectionId));
    MOCK_METHOD(DD_RESULT, ConfigureTraceParams, (DDUberTraceInstance*, DDConnectionId, const char*, size_t));
    MOCK_METHOD(DD_RESULT, RequestTrace, (DDUberTraceInstance*, DDConnectionId));
    MOCK_METHOD(DD_RESULT, CancelTrace, (DDUberTraceInstance*, DDConnectionId));
    MOCK_METHOD(DD_RESULT, CollectTrace, (DDUberTraceInstance*, DDConnectionId, uint32_t, const DDByteWriter*));
};

/// @brief A mock implementation of the RRA native API.
///
/// Unfortunately because DevDriver uses raw function pointers, static functions need to be used.
/// In order to maintain thread-safety, only one test is allowed to be using the mock at any given time.
class MockRraApi
{
public:
    /// @brief Reserves the use of the mock and populates api with pointers to the mock.
    ///
    /// Using the mock after the reservation has been released is undefined behavior.
    /// @param api The api to populate with the mock methods.
    /// @param delegate The delegate to use as the implementation of the API.
    /// @return A lock to hold until the mock is no longer needed.
    static std::unique_lock<std::mutex> UseMock(DDUberTraceApi* api, MockRraApiDelegate* delegate);

private:
    static DD_RESULT Connect(DDUberTraceInstance* instance, DDConnectionId umd_connection_id);
    static void      Disconnect(DDUberTraceInstance* instance, DDConnectionId umd_connection_id);
    static DD_RESULT EnableTracing(DDUberTraceInstance* instance, DDConnectionId umd_connection_id);
    static DD_RESULT ConfigureTraceParams(DDUberTraceInstance* instance, DDConnectionId umd_connection_id, const char* data, size_t data_size);
    static DD_RESULT RequestTrace(DDUberTraceInstance* instance, DDConnectionId umd_connection_id);
    static DD_RESULT CancelTrace(DDUberTraceInstance* instance, DDConnectionId umd_connection_id);
    static DD_RESULT CollectTrace(DDUberTraceInstance* instance, DDConnectionId umd_connection_id, uint32_t timeout_ms, const DDByteWriter* writer);

    static std::mutex          mutex_;     ///< The mutex used to reserve use of the mock.
    static MockRraApiDelegate* delegate_;  ///< The delegate to use as the implementation of the API.
};

#endif
