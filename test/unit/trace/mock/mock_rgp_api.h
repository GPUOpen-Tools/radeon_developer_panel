//=============================================================================
// Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for mock RGP native API
//=============================================================================

#ifndef RDP_TEST_UNIT_TRACE_MOCK_MOCK_RGP_API
#define RDP_TEST_UNIT_TRACE_MOCK_MOCK_RGP_API

#include <mutex>

#include <gmock/gmock.h>

#include <ddApi.h>
#include <dd_gpu_profiling_api.h>

/// @brief The object for the mock common API to delegate to.
class MockRgpApiDelegate
{
public:
    MOCK_METHOD(DD_RESULT, EnableTracing, (DDGpuProfilingInstance*, DDConnectionId, const DDGpuProfilingConfig*));
    MOCK_METHOD(void, DisableTracing, (DDGpuProfilingInstance*, DDConnectionId));
    MOCK_METHOD(DD_RESULT, ExecuteTrace, (DDGpuProfilingInstance*, DDConnectionId, const DDGpuProfilingTraceArgs*));
    MOCK_METHOD(void, AbortTrace, (DDGpuProfilingInstance*, DDConnectionId));
    MOCK_METHOD(DD_RESULT, SetSpmCounters, (DDGpuProfilingInstance*, DDConnectionId, const DDGpuProfilingSpmCounterId*, uint32_t));
    MOCK_METHOD(DD_RESULT, QueryClientProtocolVersion, (DDGpuProfilingInstance*, DDConnectionId, uint16_t*));
};

/// @brief A mock implementation of the RGP native API.
///
/// Unfortunately because DevDriver uses raw function pointers, static functions need to be used.
/// In order to maintain thread-safety, only one test is allowed to be using the mock at any given time.
class MockRgpApi
{
public:
    /// @brief Reserves the use of the mock and populates api with pointers to the mock.
    ///
    /// Using the mock after the reservation has been released is undefined behavior.
    /// @param api The api to populate with the mock methods.
    /// @param delegate The delegate to use as the implementation of the API.
    /// @return A lock to hold until the mock is no longer needed.
    static std::unique_lock<std::mutex> UseMock(DDGpuProfilingApi* api, MockRgpApiDelegate* delegate);

private:
    static DD_RESULT EnableTracing(DDGpuProfilingInstance* instance, DDConnectionId umd_connection_id, const DDGpuProfilingConfig* config);
    static void      DisableTracing(DDGpuProfilingInstance* instance, DDConnectionId umd_connection_id);
    static DD_RESULT ExecuteTrace(DDGpuProfilingInstance* instance, DDConnectionId umd_connection_id, const DDGpuProfilingTraceArgs* args);
    static void      AbortTrace(DDGpuProfilingInstance* instance, DDConnectionId umd_connection_id);
    static DD_RESULT SetSpmCounters(DDGpuProfilingInstance*           instance,
                                    DDConnectionId                    umd_connection_id,
                                    const DDGpuProfilingSpmCounterId* counters,
                                    uint32_t                          num_counters);
    static DD_RESULT QueryClientProtocolVersion(DDGpuProfilingInstance* instance, DDConnectionId umd_connection_id, uint16_t* version);

    static std::mutex          mutex_;     ///< The mutex used to reserve use of the mock.
    static MockRgpApiDelegate* delegate_;  ///< The delegate to use as the implementation of the API.
};

#endif
