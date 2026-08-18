//=============================================================================
// Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Tests for the RMV trace source.
//=============================================================================

#include <array>
#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QRegularExpression>
#include <QString>

#include <ddApi.h>
#include <ddModule.h>
#include <dd_memory_trace_api.h>

#include <rmv_trace_source.h>
#include <trace_source_factory.h>

#include <mock/mock_trace_io.h>

#include "mock/mock_module_common.h"
#include "mock/mock_rmv_api.h"

namespace devtrace
{
    static const char* kDx12DriverName = "AMD DirectX12 Driver";

    // Test data
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static const DDModuleDataContext   kMockDataContext   = reinterpret_cast<DDModuleDataContext>(0x1234);
    static const DDModuleClientContext kMockClientContext = reinterpret_cast<DDModuleClientContext>(0x4321);

    static DDMemoryTraceApi  kMockRmvApi;
    static DDModuleCommonApi kCommonApi;

    // Test suite / fixture
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class RmvTraceSourceTest : public ::testing::Test
    {
    protected:
        RmvTraceSourceTest()
        {
            common_api_lock_ = MockCommonApi::UseMock(&kCommonApi, &common_api_mock);
            rmv_api_lock_    = MockRmvApi::UseMock(&kMockRmvApi, &rmv_api_mock);
            stream_provider  = std::make_shared<MockReadWriteStreamProvider>();
        }

        // Utils
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        void MockTraceRunning(MockCommonApiDelegate& common_api, MockRmvApiDelegate& rmv_api, int common_query_times = 1, int status_query_times = 1)
        {
            EXPECT_CALL(common_api, QueryStatus(kMockClientContext)).Times(common_query_times).WillRepeatedly(::testing::Return(DD_RESULT_SUCCESS));
            EXPECT_CALL(rmv_api, QueryStatus(kMockDataContext, ::testing::Ne(nullptr)))
                .Times(status_query_times)
                .WillRepeatedly([]([[maybe_unused]] DDModuleDataContext data_context, DDMemoryTraceStatus* status) {
                    status->state = DD_MEMORY_TRACE_STATE_RUNNING;

                    return DD_RESULT_SUCCESS;
                });
        }

        std::shared_ptr<RmvTraceSource> MockSource()
        {
            return TraceSourceFactory::CreateRmvSource(kMockDataContext, &kMockRmvApi, kCommonApi, stream_provider, nullptr);
        }

        void MockTraceEnding(bool*                 has_ended,
                             int                   num_query_status_calls = 2,
                             DD_MEMORY_TRACE_STATE ended_state            = DD_MEMORY_TRACE_STATE_ENDED_APP_EXITED,
                             DD_RESULT             ended_query_result     = DD_RESULT_SUCCESS)
        {
            EXPECT_CALL(rmv_api_mock, EndTrace(kMockClientContext)).Times(1).WillOnce([=]([[maybe_unused]] DDModuleClientContext context) {
                *has_ended = true;
                return DD_RESULT_SUCCESS;
            });

            // Start in the running state but transition to ended after end trace is called
            EXPECT_CALL(common_api_mock, QueryStatus(kMockClientContext))
                .Times(num_query_status_calls - 1)
                .WillRepeatedly(::testing::Return(DD_RESULT_SUCCESS));

            EXPECT_CALL(rmv_api_mock, QueryStatus(kMockDataContext, ::testing::Ne(nullptr)))
                .Times(num_query_status_calls)
                .WillRepeatedly([=]([[maybe_unused]] DDModuleDataContext data_context, DDMemoryTraceStatus* status) {
                    status->state  = (*has_ended) ? ended_state : DD_MEMORY_TRACE_STATE_RUNNING;
                    status->result = DD_RESULT_SUCCESS;

                    return (*has_ended) ? ended_query_result : DD_RESULT_SUCCESS;
                });
        }

        // IMPORTANT: Strict mocks will make sure that any mock warning is treated as an error.
        // This avoids issues where a test might be erroneously passing.
        ::testing::StrictMock<MockCommonApiDelegate> common_api_mock;
        ::testing::StrictMock<MockRmvApiDelegate>    rmv_api_mock;

        std::shared_ptr<MockReadWriteStreamProvider> stream_provider;

    private:
        std::unique_lock<std::mutex> common_api_lock_;
        std::unique_lock<std::mutex> rmv_api_lock_;
    };

    // Factory tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RmvTraceSourceTest, FactoryCreatesNullWithBadDataContext)
    {
        auto factory_result = TraceSourceFactory::CreateRmvSource(DD_API_INVALID_HANDLE, &kMockRmvApi, kCommonApi, stream_provider);
        EXPECT_EQ(factory_result, nullptr);
    }

    TEST_F(RmvTraceSourceTest, FactoryCreatesNullWithBadApi)
    {
        auto factory_result = TraceSourceFactory::CreateRmvSource(kMockDataContext, nullptr, kCommonApi, stream_provider);
        EXPECT_EQ(factory_result, nullptr);
    }

    TEST_F(RmvTraceSourceTest, FactoryCreatesNullWithBadStreamProvider)
    {
        auto factory_result = TraceSourceFactory::CreateRmvSource(kMockDataContext, &kMockRmvApi, kCommonApi, nullptr);
        EXPECT_EQ(factory_result, nullptr);
    }

    TEST_F(RmvTraceSourceTest, FactoryCreatesRmvTraceSource)
    {
        auto factory_result = TraceSourceFactory::CreateRmvSource(kMockDataContext, &kMockRmvApi, kCommonApi, stream_provider);
        EXPECT_NE(factory_result, nullptr);
    }

    // API tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RmvTraceSourceTest, SelectsDX12Api)
    {
        auto source = MockSource();
        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kDirectX12);
    }

    TEST_F(RmvTraceSourceTest, SelectsDX11Api)
    {
        auto source = MockSource();
        source->Update("AMD DirectX10/11 Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kDirectX11);
    }

    TEST_F(RmvTraceSourceTest, SelectsDX9Api)
    {
        auto source = MockSource();
        source->Update("AMD DirectX9 Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kDirectX9);
    }

    TEST_F(RmvTraceSourceTest, SelectsVulkanApi)
    {
        auto source = MockSource();
        source->Update("AMD Vulkan Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kVulkan);
    }

    TEST_F(RmvTraceSourceTest, SelectsOpenClApi)
    {
        auto source = MockSource();
        source->Update("AMD OpenCL Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kOpenCl);
    }

    TEST_F(RmvTraceSourceTest, SelectsHipApi)
    {
        auto source = MockSource();
        source->Update("AMD HIP Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kHip);
    }

    TEST_F(RmvTraceSourceTest, SelectsOpenGlApi)
    {
        auto source = MockSource();
        source->Update("AMD OpenGL Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kOpenGl);
    }

    TEST_F(RmvTraceSourceTest, SelectsUnknownApi)
    {
        auto source = MockSource();
        source->Update("AMD DirectX-360 Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kUnknown);
    }

    TEST_F(RmvTraceSourceTest, CallsStatusUpdateForApiUpdate)
    {
        bool called_update = false;
        auto source        = MockSource();

        source->SetStatusCallback([&]([[maybe_unused]] const TraceSourceStatus& status) { called_update = true; });
        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);

        EXPECT_TRUE(called_update);
    }

    TEST_F(RmvTraceSourceTest, DoesntCallStatusUpdateForNoApiChange)
    {
        bool called_update = false;
        auto source        = MockSource();

        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);
        source->SetStatusCallback([&]([[maybe_unused]] const TraceSourceStatus& status) { called_update = true; });
        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);

        EXPECT_FALSE(called_update);
    }

    // Status update tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RmvTraceSourceTest, UpdatesStageToDisabledOpenCl)
    {
        auto source = MockSource();
        source->Update("AMD OpenCL Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDisabled);
    }

    TEST_F(RmvTraceSourceTest, UpdatesStageToDisabledHip)
    {
        auto source = MockSource();
        source->Update("AMD HIP Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDisabled);
    }

    TEST_F(RmvTraceSourceTest, UpdatesStageToDisabledUnsupported)
    {
        EXPECT_CALL(common_api_mock, QueryStatus(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_COMMON_UNSUPPORTED));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDisabled);
    }

    TEST_F(RmvTraceSourceTest, UpdatesStageToDisabledCommonVersionMismatch)
    {
        EXPECT_CALL(common_api_mock, QueryStatus(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_COMMON_VERSION_MISMATCH));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDisabled);
    }

    TEST_F(RmvTraceSourceTest, UpdatesStageToDisabledGenericVersionMismatch)
    {
        EXPECT_CALL(common_api_mock, QueryStatus(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_DD_GENERIC_VERSION_MISMATCH));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDisabled);
    }

    TEST_F(RmvTraceSourceTest, UpdatesStageToErrorUnknownStatus)
    {
        EXPECT_CALL(common_api_mock, QueryStatus(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kError);
    }

    TEST_F(RmvTraceSourceTest, UpdatesStageToErrorErrorStatus)
    {
        EXPECT_CALL(common_api_mock, QueryStatus(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_DD_GENERIC_NOT_READY));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kError);
    }

    TEST_F(RmvTraceSourceTest, UpdatesStageToRunning)
    {
        MockTraceRunning(common_api_mock, rmv_api_mock);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kCapturing);
    }

    TEST_F(RmvTraceSourceTest, UpdatesStageToDoneWhenTracingDone)
    {
        EXPECT_CALL(common_api_mock, QueryStatus(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        EXPECT_CALL(rmv_api_mock, QueryStatus(kMockDataContext, ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([]([[maybe_unused]] DDModuleDataContext data_context, DDMemoryTraceStatus* status) {
                status->state = DD_MEMORY_TRACE_STATE_ENDED_APP_EXITED;

                return DD_RESULT_SUCCESS;
            });

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDone);
    }

    TEST_F(RmvTraceSourceTest, UpdatesStageToDoneQueryError)
    {
        EXPECT_CALL(common_api_mock, QueryStatus(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        EXPECT_CALL(rmv_api_mock, QueryStatus(kMockDataContext, ::testing::Ne(nullptr))).Times(1).WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDone);
    }

    TEST_F(RmvTraceSourceTest, UpdateStageEmitsStatus)
    {
        bool called_update = false;
        MockTraceRunning(common_api_mock, rmv_api_mock);

        auto source = MockSource();
        source->SetStatusCallback([&]([[maybe_unused]] const TraceSourceStatus& status) { called_update = true; });

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kCapturing);
        EXPECT_TRUE(called_update);
    }

    TEST_F(RmvTraceSourceTest, UpdateStageEmitsOnlyTransitions)
    {
        bool called_update = false;
        MockTraceRunning(common_api_mock, rmv_api_mock, 2, 2);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kCapturing);

        source->SetStatusCallback([&]([[maybe_unused]] const TraceSourceStatus& status) { called_update = true; });
        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_FALSE(called_update);
    }

    // Insert snapshot tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RmvTraceSourceTest, InsertSnapshotFailsNotConnected)
    {
        auto source = MockSource();
        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->AddMarker("Marker"), Result::kFailure);
    }

    TEST_F(RmvTraceSourceTest, InsertSnapshotFailsIfAborted)
    {
        EXPECT_CALL(rmv_api_mock, EndTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        MockTraceRunning(common_api_mock, rmv_api_mock);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestAbortTrace(), Result::kSuccess);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDone);

        EXPECT_EQ(source->AddMarker("Marker"), Result::kFailure);
    }

    TEST_F(RmvTraceSourceTest, InsertSnapshotFailsAPIError)
    {
        EXPECT_CALL(rmv_api_mock, InsertSnapshot(kMockClientContext, ::testing::StrEq("Marker"))).Times(1).WillOnce(::testing::Return(DD_RESULT_UNKNOWN));
        MockTraceRunning(common_api_mock, rmv_api_mock);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->AddMarker("Marker"), Result::kFailure);
    }

    TEST_F(RmvTraceSourceTest, InsertSnapshotSucceeds)
    {
        EXPECT_CALL(rmv_api_mock, InsertSnapshot(kMockClientContext, ::testing::StrEq("Marker"))).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        MockTraceRunning(common_api_mock, rmv_api_mock);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->AddMarker("Marker"), Result::kSuccess);
    }

    // Abort tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RmvTraceSourceTest, AbortFailsWhenNotConnected)
    {
        auto source = MockSource();
        EXPECT_EQ(source->RequestAbortTrace(), Result::kFailure);
    }

    TEST_F(RmvTraceSourceTest, AbortFailsWhenNativeApiFails)
    {
        EXPECT_CALL(rmv_api_mock, EndTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_UNKNOWN));
        MockTraceRunning(common_api_mock, rmv_api_mock);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestAbortTrace(), Result::kFailure);
    }

    TEST_F(RmvTraceSourceTest, AbortSucceeds)
    {
        EXPECT_CALL(rmv_api_mock, EndTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        MockTraceRunning(common_api_mock, rmv_api_mock);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestAbortTrace(), Result::kSuccess);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDone);
    }

    TEST_F(RmvTraceSourceTest, AbortSucceedsOnlyOnce)
    {
        EXPECT_CALL(rmv_api_mock, EndTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        MockTraceRunning(common_api_mock, rmv_api_mock);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestAbortTrace(), Result::kSuccess);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDone);

        EXPECT_EQ(source->RequestAbortTrace(), Result::kFailure);
    }

    TEST_F(RmvTraceSourceTest, AbortFailureDoesntUpdateStage)
    {
        EXPECT_CALL(rmv_api_mock, EndTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_UNKNOWN));
        MockTraceRunning(common_api_mock, rmv_api_mock);

        bool called_update = false;
        auto source        = MockSource();

        source->Update(kDx12DriverName, kMockClientContext);
        source->SetStatusCallback([&]([[maybe_unused]] const TraceSourceStatus& status) { called_update = true; });

        EXPECT_EQ(source->RequestAbortTrace(), Result::kFailure);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kCapturing);
        EXPECT_FALSE(called_update);
    }

    TEST_F(RmvTraceSourceTest, AbortSuccessUpdatesStage)
    {
        EXPECT_CALL(rmv_api_mock, EndTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        MockTraceRunning(common_api_mock, rmv_api_mock);

        bool called_update = false;
        auto source        = MockSource();

        source->Update(kDx12DriverName, kMockClientContext);
        source->SetStatusCallback([&]([[maybe_unused]] const TraceSourceStatus& status) { called_update = true; });

        EXPECT_EQ(source->RequestAbortTrace(), Result::kSuccess);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDone);
        EXPECT_TRUE(called_update);
    }

    TEST_F(RmvTraceSourceTest, AbortSuccessCallsCallback)
    {
        EXPECT_CALL(rmv_api_mock, EndTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        MockTraceRunning(common_api_mock, rmv_api_mock);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kAborted, status);

            EXPECT_EQ("", path);
            EXPECT_EQ(0, stream_provider->GetData().size());
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
        });

        EXPECT_EQ(source->RequestAbortTrace(), Result::kSuccess);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDone);
    }

    // Dump tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RmvTraceSourceTest, CantDumpIfStopFails)
    {
        EXPECT_CALL(rmv_api_mock, EndTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_UNKNOWN));
        MockTraceRunning(common_api_mock, rmv_api_mock);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestDump(), Result::kFailure);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kCapturing);
    }

    TEST_F(RmvTraceSourceTest, CantDumpIfRunning)
    {
        EXPECT_CALL(rmv_api_mock, EndTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        MockTraceRunning(common_api_mock, rmv_api_mock, 1, 2);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestDump(), Result::kFailure);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kError);
    }

    TEST_F(RmvTraceSourceTest, CantDumpIfAborted)
    {
        EXPECT_CALL(rmv_api_mock, EndTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        MockTraceRunning(common_api_mock, rmv_api_mock);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestAbortTrace(), Result::kSuccess);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDone);

        EXPECT_EQ(source->RequestDump(), Result::kFailure);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDone);
    }

    TEST_F(RmvTraceSourceTest, WontDumpOnAppExitIfAborted)
    {
        EXPECT_CALL(rmv_api_mock, EndTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        MockTraceRunning(common_api_mock, rmv_api_mock);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kAborted, status);

            EXPECT_EQ("", path);
            EXPECT_EQ(0, stream_provider->GetData().size());
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
        });

        EXPECT_EQ(source->RequestAbortTrace(), Result::kSuccess);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDone);

        source->Update(nullptr, DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDisconnected);
    }

    TEST_F(RmvTraceSourceTest, CantDumpIfQueryFails)
    {
        bool has_ended = false;
        MockTraceEnding(&has_ended, 2, DD_MEMORY_TRACE_STATE_UNKNOWN, DD_RESULT_UNKNOWN);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestDump(), Result::kFailure);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kError);
    }

    TEST_F(RmvTraceSourceTest, CantDumpIfUnknownState)
    {
        bool has_ended = false;
        MockTraceEnding(&has_ended, 2, DD_MEMORY_TRACE_STATE_UNKNOWN);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestDump(), Result::kFailure);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kError);
    }

    TEST_F(RmvTraceSourceTest, DumpFailsIfByteWriterFails)
    {
        bool has_ended = false;
        MockTraceEnding(&has_ended);

        EXPECT_CALL(rmv_api_mock, TransferTraceData(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr), false))
            .Times(1)
            .WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        auto source = MockSource();
        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kError, status);

            EXPECT_EQ("", path);
            EXPECT_EQ(0, stream_provider->GetData().size());
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
        });

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestDump(), Result::kFailure);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kError);
    }

    TEST_F(RmvTraceSourceTest, DumpFailsIfInitialResultIsBad)
    {
        bool has_ended = false;
        MockTraceEnding(&has_ended);
        stream_provider->SetNextStreamsIsNullptr();

        auto source = MockSource();
        EXPECT_CALL(rmv_api_mock, TransferTraceData(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr), false))
            .Times(1)
            .WillOnce([&]([[maybe_unused]] DDModuleDataContext    data_context,
                          [[maybe_unused]] const DDRdfFileWriter* writer,
                          const DDIOHeartbeat*                    heartbeat,
                          [[maybe_unused]] bool                   compressed) {
                DD_RESULT result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_UNKNOWN, DD_IO_STATUS_BEGIN, 0);
                EXPECT_EQ(DD_RESULT_UNKNOWN, result);

                EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

                return result;
            });

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kError, status);

            EXPECT_EQ("", path);
            EXPECT_EQ(0, stream_provider->GetData().size());
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
        });

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestDump(), Result::kFailure);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kError);
    }

    TEST_F(RmvTraceSourceTest, DumpFailsIfProvderGivesNullStream)
    {
        bool has_ended = false;
        MockTraceEnding(&has_ended);
        stream_provider->SetNextStreamsIsNullptr();

        auto source = MockSource();
        EXPECT_CALL(rmv_api_mock, TransferTraceData(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr), false))
            .Times(1)
            .WillOnce([&]([[maybe_unused]] DDModuleDataContext    data_context,
                          [[maybe_unused]] const DDRdfFileWriter* writer,
                          const DDIOHeartbeat*                    heartbeat,
                          [[maybe_unused]] bool                   compressed) {
                DD_RESULT result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_BEGIN, 123);
                EXPECT_EQ(DD_RESULT_UNKNOWN, result);

                EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

                return result;
            });

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kError, status);

            EXPECT_EQ(stream_provider->GetPath(), path);
            EXPECT_EQ(0, stream_provider->GetData().size());
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
        });

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestDump(), Result::kFailure);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kError);
    }

    TEST_F(RmvTraceSourceTest, DumpFailsWhenWriteFails)
    {
        bool has_ended = false;
        MockTraceEnding(&has_ended);
        stream_provider->SetNextStreamsIsBad();

        auto        source          = MockSource();
        std::string string_to_write = "hello world";

        // Use the byte writer to write out a message
        EXPECT_CALL(rmv_api_mock, TransferTraceData(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr), false))
            .Times(1)
            .WillOnce([&]([[maybe_unused]] DDModuleDataContext data_context,
                          const DDRdfFileWriter*               writer,
                          const DDIOHeartbeat*                 heartbeat,
                          [[maybe_unused]] bool                compressed) {
                size_t    write_length = string_to_write.length() + 1;
                DD_RESULT result       = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_BEGIN, write_length);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                EXPECT_EQ(TraceSourceStage::kDumping, source->GetStatus().GetStage());
                EXPECT_EQ(write_length, source->GetStatus().total_bytes_to_dump);
                EXPECT_EQ(0.0, source->GetStatus().stage_progress);

                result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_WRITE, 5);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                EXPECT_FLOAT_EQ(0.416666667f, source->GetStatus().stage_progress);
                EXPECT_FLOAT_EQ(5.f, source->GetStatus().num_bytes_dumped);

                std::int64_t bytes_written = 0;
                EXPECT_EQ(1, writer->pfnFileWrite(writer->pUserData, 5, string_to_write.c_str(), &bytes_written));
                EXPECT_EQ(0, bytes_written);

                result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_UNKNOWN, DD_IO_STATUS_END, 0);

                return result;
            });

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kError, status);

            EXPECT_EQ(stream_provider->GetPath(), path);
            EXPECT_EQ(0, stream_provider->GetData().size());
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
        });

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestDump(), Result::kFailure);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kError);
    }

    TEST_F(RmvTraceSourceTest, DumpsWhenRequested)
    {
        bool has_ended = false;
        MockTraceEnding(&has_ended);

        auto        source          = MockSource();
        std::string string_to_write = "hello world";
        size_t      write_length    = string_to_write.length() + 1;

        EXPECT_CALL(rmv_api_mock, TransferTraceData(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr), false))
            .Times(1)
            .WillOnce([&]([[maybe_unused]] DDModuleDataContext data_context,
                          const DDRdfFileWriter*               writer,
                          const DDIOHeartbeat*                 heartbeat,
                          [[maybe_unused]] bool                compressed) {
                DD_RESULT result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_BEGIN, write_length);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                EXPECT_EQ(TraceSourceStage::kDumping, source->GetStatus().GetStage());
                EXPECT_EQ(write_length, source->GetStatus().total_bytes_to_dump);
                EXPECT_EQ(0.0, source->GetStatus().stage_progress);

                result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_WRITE, 5);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                EXPECT_FLOAT_EQ(0.416666667f, source->GetStatus().stage_progress);
                EXPECT_FLOAT_EQ(5.f, source->GetStatus().num_bytes_dumped);

                std::int64_t bytes_written = 0;
                EXPECT_EQ(0, writer->pfnFileWrite(writer->pUserData, 5, string_to_write.c_str(), &bytes_written));
                EXPECT_EQ(5, bytes_written);

                result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_WRITE, write_length - 5);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                EXPECT_FLOAT_EQ(1.0f, source->GetStatus().stage_progress);
                EXPECT_FLOAT_EQ(write_length, source->GetStatus().num_bytes_dumped);

                bytes_written = 0;
                EXPECT_EQ(0, writer->pfnFileWrite(writer->pUserData, write_length - 5, string_to_write.c_str() + 5, &bytes_written));
                EXPECT_EQ(write_length - 5, bytes_written);

                result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_END, 0);

                return result;
            });

        EXPECT_CALL(rmv_api_mock, ClearTrace(kMockDataContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kCompleted, status);

            EXPECT_EQ(stream_provider->GetPath(), path);
            EXPECT_EQ(write_length, stream_provider->GetData().size());
            EXPECT_STREQ(string_to_write.c_str(), stream_provider->GetData().data());
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
        });

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestDump(), Result::kSuccess);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDone);
    }

    TEST_F(RmvTraceSourceTest, WritesEvenIfInitialByteEstimateIsWrong)
    {
        bool has_ended = false;
        MockTraceEnding(&has_ended);

        auto        source          = MockSource();
        std::string string_to_write = "hello world";
        size_t      write_length    = string_to_write.length() + 1;

        EXPECT_CALL(rmv_api_mock, TransferTraceData(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr), false))
            .Times(1)
            .WillOnce([&]([[maybe_unused]] DDModuleDataContext data_context,
                          const DDRdfFileWriter*               writer,
                          const DDIOHeartbeat*                 heartbeat,
                          [[maybe_unused]] bool                compressed) {
                DD_RESULT result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_BEGIN, 5);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                EXPECT_EQ(TraceSourceStage::kDumping, source->GetStatus().GetStage());
                EXPECT_EQ(5, source->GetStatus().total_bytes_to_dump);
                EXPECT_EQ(0.0, source->GetStatus().stage_progress);

                result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_WRITE, 5);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                EXPECT_EQ(5, source->GetStatus().total_bytes_to_dump);
                EXPECT_FLOAT_EQ(1.0, source->GetStatus().stage_progress);
                EXPECT_FLOAT_EQ(5, source->GetStatus().num_bytes_dumped);

                std::int64_t bytes_written = 0;
                EXPECT_EQ(0, writer->pfnFileWrite(writer->pUserData, 5, string_to_write.c_str(), &bytes_written));
                EXPECT_EQ(5, bytes_written);

                result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_WRITE, write_length - 5);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                EXPECT_EQ(write_length, source->GetStatus().total_bytes_to_dump);
                EXPECT_FLOAT_EQ(1.0, source->GetStatus().stage_progress);
                EXPECT_FLOAT_EQ(write_length, source->GetStatus().num_bytes_dumped);

                bytes_written = 0;
                EXPECT_EQ(0, writer->pfnFileWrite(writer->pUserData, write_length - 5, string_to_write.c_str() + 5, &bytes_written));
                EXPECT_EQ(write_length - 5, bytes_written);

                result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_END, 0);

                return result;
            });

        EXPECT_CALL(rmv_api_mock, ClearTrace(kMockDataContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kCompleted, status);

            EXPECT_EQ(stream_provider->GetPath(), path);
            EXPECT_EQ(write_length, stream_provider->GetData().size());
            EXPECT_STREQ(string_to_write.c_str(), stream_provider->GetData().data());
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
        });

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestDump(), Result::kSuccess);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDone);
    }

    TEST_F(RmvTraceSourceTest, ReadCallbacksAreWiredProperly)
    {
        bool has_ended = false;
        MockTraceEnding(&has_ended);

        auto        source          = MockSource();
        std::string string_to_write = "hello world";
        size_t      write_length    = string_to_write.length() + 1;

        EXPECT_CALL(rmv_api_mock, TransferTraceData(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr), false))
            .Times(1)
            .WillOnce([&]([[maybe_unused]] DDModuleDataContext data_context,
                          const DDRdfFileWriter*               writer,
                          const DDIOHeartbeat*                 heartbeat,
                          [[maybe_unused]] bool                compressed) {
                DD_RESULT result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_BEGIN, write_length);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);
                EXPECT_EQ(TraceSourceStage::kDumping, source->GetStatus().GetStage());

                result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_WRITE, write_length);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                std::int64_t bytes_written = 0;
                EXPECT_EQ(0, writer->pfnFileWrite(writer->pUserData, write_length, string_to_write.c_str(), &bytes_written));
                EXPECT_EQ(write_length, bytes_written);

                std::int64_t value;
                EXPECT_EQ(0, writer->pfnFileGetSize(writer->pUserData, &value));
                EXPECT_EQ(write_length, value);

                value = 0;
                EXPECT_EQ(0, writer->pfnFileTell(writer->pUserData, &value));
                EXPECT_EQ(write_length, value);

                EXPECT_EQ(0, writer->pfnFileSeek(writer->pUserData, 0));

                std::array<char, 12> read_buffer;
                EXPECT_EQ(0, writer->pfnFileRead(writer->pUserData, write_length, read_buffer.data(), &value));
                EXPECT_EQ(string_to_write, std::string(read_buffer.data()));
                EXPECT_EQ(write_length, value);

                result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_END, 0);

                return result;
            });

        EXPECT_CALL(rmv_api_mock, ClearTrace(kMockDataContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kCompleted, status);

            EXPECT_EQ(stream_provider->GetPath(), path);
            EXPECT_EQ(write_length, stream_provider->GetData().size());
            EXPECT_STREQ(string_to_write.c_str(), stream_provider->GetData().data());
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
        });

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestDump(), Result::kSuccess);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDone);
    }

    TEST_F(RmvTraceSourceTest, UpdateAfterDumpFailDoesntUpdateStage)
    {
        bool has_ended = false;
        MockTraceEnding(&has_ended, 3);

        EXPECT_CALL(rmv_api_mock, TransferTraceData(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr), false))
            .Times(1)
            .WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestDump(), Result::kFailure);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kError);

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kError);
    }

    TEST_F(RmvTraceSourceTest, UpdateAfterDumpSuccessDoesntUpdateStage)
    {
        bool has_ended = false;
        MockTraceEnding(&has_ended, 3);

        EXPECT_CALL(rmv_api_mock, TransferTraceData(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr), false))
            .Times(1)
            .WillOnce(::testing::Return(DD_RESULT_SUCCESS));

        EXPECT_CALL(rmv_api_mock, ClearTrace(kMockDataContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->RequestDump(), Result::kSuccess);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDone);

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDone);
    }

    TEST_F(RmvTraceSourceTest, DumpsWhenClientDisconnects)
    {
        bool called_query = false;
        EXPECT_CALL(common_api_mock, QueryStatus(kMockClientContext)).Times(1).WillRepeatedly(::testing::Return(DD_RESULT_SUCCESS));
        EXPECT_CALL(rmv_api_mock, QueryStatus(kMockDataContext, ::testing::Ne(nullptr)))
            .Times(2)
            .WillRepeatedly([&]([[maybe_unused]] DDModuleDataContext data_context, DDMemoryTraceStatus* status) {
                status->state = !called_query ? DD_MEMORY_TRACE_STATE_RUNNING : DD_MEMORY_TRACE_STATE_ENDED_APP_EXITED;

                called_query = true;
                return DD_RESULT_SUCCESS;
            });

        EXPECT_CALL(rmv_api_mock, TransferTraceData(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr), false))
            .Times(1)
            .WillOnce(::testing::Return(DD_RESULT_SUCCESS));

        EXPECT_CALL(rmv_api_mock, ClearTrace(kMockDataContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDisconnected);
    }

}  // namespace devtrace
