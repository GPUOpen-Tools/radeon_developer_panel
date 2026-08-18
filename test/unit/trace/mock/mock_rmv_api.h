//=============================================================================
// Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for mock RMV native API.
//=============================================================================

#ifndef RDP_TEST_UNIT_TRACE_MOCK_MOCK_RMV_API_H_
#define RDP_TEST_UNIT_TRACE_MOCK_MOCK_RMV_API_H_

#include <mutex>

#include <gmock/gmock.h>

#include <ddApi.h>
#include <dd_memory_trace_api.h>

/// @brief The object for the mock native API to delegate to.
class MockRmvApiDelegate
{
public:
    MOCK_METHOD(DD_RESULT, EnableTracing, (DDMemoryTraceInstance*, DDConnectionId, DDProcessId, bool));
    MOCK_METHOD(DD_RESULT, DisableTracing, (DDMemoryTraceInstance*, DDConnectionId));
    MOCK_METHOD(DD_RESULT, EndTracing, (DDMemoryTraceInstance*, DDConnectionId, bool));
    MOCK_METHOD(DD_RESULT, DumpTrace, (DDMemoryTraceInstance*, DDConnectionId, bool));
    MOCK_METHOD(DD_RESULT, AbortTrace, (DDMemoryTraceInstance*, DDConnectionId, bool));
    MOCK_METHOD(DD_RESULT, InsertSnapshot, (DDMemoryTraceInstance*, DDConnectionId, const char*));
    MOCK_METHOD(DD_RESULT, ClearTrace, (DDMemoryTraceInstance*, DDConnectionId));
    MOCK_METHOD(DD_RESULT, QueryStatus, (DDMemoryTraceInstance*, DDConnectionId, DDMemoryTraceStatus*));
    MOCK_METHOD(DD_RESULT, TransferTraceData, (DDMemoryTraceInstance*, DDConnectionId, const DDRdfFileWriter*, const DDIOHeartbeat*, bool));
};

/// @brief A mock implementation of the RMV native API.
///
/// Unfortunately because DevDriver uses raw function pointers, static functions need to be used.
/// In order to maintain thread-safety, only one test is allowed to be using the mock at any given time.
class MockRmvApi
{
public:
    /// @brief Reserves the use of the mock and populates api with pointers to the mock.
    ///
    /// Using the mock after the reservation has been released is undefined behavior.
    /// @param api The api to populate with the mock methods.
    /// @param delegate The delegate to use as the implementation of the API.
    /// @return A lock to hold until the mock is no longer needed.
    static std::unique_lock<std::mutex> UseMock(DDMemoryTraceApi* api, MockRmvApiDelegate* delegate);

private:
    static DD_RESULT EnableTracing(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id, DDProcessId process_id, bool use_kmd);
    static DD_RESULT DisableTracing(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id);
    static DD_RESULT EndTracing(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id, bool is_client_initialized);
    static DD_RESULT DumpTrace(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id, bool is_client_initialized);
    static DD_RESULT AbortTrace(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id, bool is_client_initialized);
    static DD_RESULT InsertSnapshot(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id, const char* snapshot_name);
    static DD_RESULT ClearTrace(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id);
    static DD_RESULT QueryStatus(DDMemoryTraceInstance* instance, DDConnectionId umd_connection_id, DDMemoryTraceStatus* status);
    static DD_RESULT TransferTraceData(DDMemoryTraceInstance* instance,
                                       DDConnectionId         umd_connection_id,
                                       const DDRdfFileWriter* file_writer,
                                       const DDIOHeartbeat*   heartbeat,
                                       bool                   compressed);

    static std::mutex          mutex_;     ///< The mutex used to reserve use of the mock.
    static MockRmvApiDelegate* delegate_;  ///< The delegate to use as the implementation of the API.
};

#endif
