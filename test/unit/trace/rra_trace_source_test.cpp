// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Tests for the RRA trace source.

#include <chrono>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include <UberTraceModule.h>
#include <ddApi.h>

#include <rra_trace_source.h>
#include <system_info_cache.h>
#include <trace_source_factory.h>

#include <json_utils.h>
#include <test_wait.h>

#include "mock/mock_module_common.h"
#include "mock/mock_rra_api.h"
#include "mock/mock_system_info_cache.h"
#include "mock/mock_trace_io.h"

namespace devtrace
{
    static const char* kDx12DriverName = "AMD DirectX12 Driver";

    // Test data
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static const DDModuleSystemContext kMockSystemContext = reinterpret_cast<DDModuleSystemContext>(0x1234);
    static const DDModuleClientContext kMockClientContext = reinterpret_cast<DDModuleClientContext>(0x4321);

    static DDUberTraceApi    kMockRraApi;
    static DDModuleCommonApi kMockCommonApi;

    // Test suite / fixture
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class RraTraceSourceTest : public ::testing::Test
    {
    protected:
        RraTraceSourceTest()
        {
            JsonUtils::LoadJsonFile(SYSTEM_INFO_V4_JSON_FILENAME, system_info_json_string);
            JsonUtils::LoadJsonFile(RRA_UBER_TRACE_CONFIG_JSON_FILENAME, uber_trace_config_json_string);

            rra_api_lock_     = MockRraApi::UseMock(&kMockRraApi, &rra_api_mock);
            system_info_cache = std::make_shared<MockSystemInfoCache>(kMockCommonApi);
            stream_provider   = std::make_shared<MockReadWriteStreamProvider>();
        }

        // Utils
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        std::shared_ptr<RraTraceSource> MockSource()
        {
            return TraceSourceFactory::CreateRraSource(kMockSystemContext, &kMockRraApi, stream_provider, system_info_cache, nullptr);
        }

        // IMPORTANT: Strict mocks will make sure that any mock warning is treated as an error.
        // This avoids issues where a test might be erroneously passing.
        ::testing::StrictMock<MockCommonApiDelegate> common_api_mock;
        ::testing::StrictMock<MockRraApiDelegate>    rra_api_mock;

        std::shared_ptr<MockSystemInfoCache>         system_info_cache;
        std::shared_ptr<MockReadWriteStreamProvider> stream_provider;

        void MockQuerySystemInfo()
        {
            EXPECT_CALL(*system_info_cache, GetSystemInfo(::testing::Eq(kMockSystemContext), ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
                .Times(1)
                .WillOnce([&](DDModuleSystemContext, void* userdata, PFN_ddReceiveText receive_text) {
                    receive_text(userdata, system_info_json_string.c_str());

                    return DD_RESULT_SUCCESS;
                });
        }

        void ExpectUberTraceConfigUpdate(uint8_t times = 1, DDModuleClientContext client_context = kMockClientContext)
        {
            EXPECT_CALL(rra_api_mock, UpdateParams(client_context, ::testing::Ne(nullptr), uber_trace_config_json_string.length() + 1))
                .Times(times)
                .WillRepeatedly([&](DDModuleClientContext, const void* data, size_t) {
                    EXPECT_STREQ(uber_trace_config_json_string.c_str(), reinterpret_cast<const char*>(data));
                    return DD_RESULT_SUCCESS;
                });
        }

        static const std::string& GetUberTraceConfig()
        {
            return uber_trace_config_json_string;
        }

    private:
        static std::string system_info_json_string;
        static std::string uber_trace_config_json_string;

        std::unique_lock<std::mutex> rra_api_lock_;
    };

    std::string RraTraceSourceTest::system_info_json_string;
    std::string RraTraceSourceTest::uber_trace_config_json_string;

    // Factory tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RraTraceSourceTest, FactoryCreatesNullWithBadDataContext)
    {
        auto factory_result = TraceSourceFactory::CreateRraSource(DD_API_INVALID_HANDLE, &kMockRraApi, stream_provider, system_info_cache);
        EXPECT_EQ(factory_result, nullptr);
    }

    TEST_F(RraTraceSourceTest, FactoryCreatesNullWithBadApi)
    {
        auto factory_result = TraceSourceFactory::CreateRraSource(kMockSystemContext, nullptr, stream_provider, system_info_cache);
        EXPECT_EQ(factory_result, nullptr);
    }

    TEST_F(RraTraceSourceTest, FactoryCreatesNullWithBadStreamProvider)
    {
        auto factory_result = TraceSourceFactory::CreateRraSource(kMockSystemContext, &kMockRraApi, nullptr, system_info_cache);
        EXPECT_EQ(factory_result, nullptr);
    }

    TEST_F(RraTraceSourceTest, FactoryCreatesNullWithBadSysInfoCache)
    {
        auto factory_result = TraceSourceFactory::CreateRraSource(kMockSystemContext, &kMockRraApi, stream_provider, nullptr);
        EXPECT_EQ(factory_result, nullptr);
    }

    TEST_F(RraTraceSourceTest, FactoryCreatesRraTraceSource)
    {
        MockQuerySystemInfo();
        auto factory_result = TraceSourceFactory::CreateRraSource(kMockSystemContext, &kMockRraApi, stream_provider, system_info_cache);

        EXPECT_NE(factory_result, nullptr);
    }

    // API tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RraTraceSourceTest, SelectsDX12Api)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kDirectX12);
    }

    TEST_F(RraTraceSourceTest, SelectsDX11Api)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD DirectX10/11 Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kDirectX11);
    }

    TEST_F(RraTraceSourceTest, SelectsDX9Api)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD DirectX9 Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kDirectX9);
    }

    TEST_F(RraTraceSourceTest, SelectsVulkanApi)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD Vulkan Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kVulkan);
    }

    TEST_F(RraTraceSourceTest, SelectsOpenClApi)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD OpenCL Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kOpenCl);
    }

    TEST_F(RraTraceSourceTest, SelectsHipApi)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD HIP Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kHip);
    }

    TEST_F(RraTraceSourceTest, SelectsUnknownApi)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD DirectX-360 Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kUnknown);
    }

    TEST_F(RraTraceSourceTest, CallsStatusUpdateForApiUpdate)
    {
        MockQuerySystemInfo();

        bool called_update = false;
        auto source        = MockSource();

        source->SetStatusCallback([&](const TraceSourceStatus&) { called_update = true; });
        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);

        EXPECT_TRUE(called_update);
    }

    TEST_F(RraTraceSourceTest, DoesntCallStatusUpdateForNoApiChange)
    {
        MockQuerySystemInfo();

        bool called_update = false;
        auto source        = MockSource();

        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);
        source->SetStatusCallback([&](const TraceSourceStatus&) { called_update = true; });
        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);

        EXPECT_FALSE(called_update);
    }

    // Status update tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RraTraceSourceTest, UpdatesStageToDisabledOpenCl)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD OpenCL Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, UpdatesStageToDisabledHip)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD HIP Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, StaysDisconnectedOpenCl)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD OpenCL Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, StaysDisconnectedHip)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD HIP Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, UpdatesStageToDisconnectedAfterOpenCl)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD OpenCL Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());

        source->Update(nullptr, DD_API_INVALID_HANDLE);
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, UpdatesStageToDisconnectedAfterHip)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD HIP Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());

        source->Update(nullptr, DD_API_INVALID_HANDLE);
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, StaysDisabledOpenCl)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD OpenCL Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());

        source->Update("AMD OpenCL Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, StaysDisabledHip)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD HIP Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());

        source->Update("AMD HIP Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, UpdateStageEmitsStatus)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        bool called_update = false;
        auto source        = MockSource();
        source->SetStatusCallback([&](const TraceSourceStatus&) { called_update = true; });

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
        EXPECT_TRUE(called_update);
    }

    TEST_F(RraTraceSourceTest, ErrorsWhenCantUpdateTraceConfig)
    {
        MockQuerySystemInfo();

        EXPECT_CALL(rra_api_mock, UpdateParams(kMockClientContext, ::testing::Ne(nullptr), GetUberTraceConfig().size() + 1))
            .Times(1)
            .WillRepeatedly(::testing::Return(DD_RESULT_UNKNOWN));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kError, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, StaysInErrorStateWhenConnected)
    {
        MockQuerySystemInfo();

        EXPECT_CALL(rra_api_mock, UpdateParams(kMockClientContext, ::testing::Ne(nullptr), GetUberTraceConfig().length() + 1))
            .Times(1)
            .WillRepeatedly(::testing::Return(DD_RESULT_UNKNOWN));

        auto source = MockSource();

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kError, source->GetStatus().GetStage());

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kError, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, ResetsFromErrorWhenDisconnected)
    {
        MockQuerySystemInfo();

        EXPECT_CALL(rra_api_mock, UpdateParams(kMockClientContext, ::testing::Ne(nullptr), GetUberTraceConfig().length() + 1))
            .Times(1)
            .WillRepeatedly(::testing::Return(DD_RESULT_UNKNOWN));

        auto source = MockSource();

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kError, source->GetStatus().GetStage());

        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, UpdateStageEmitsOnlyTransitions)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate(2);

        bool called_update = false;
        auto source        = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());

        source->SetStatusCallback([&](const TraceSourceStatus&) { called_update = true; });
        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_FALSE(called_update);
    }

    // Hardware support test
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RraTraceSourceTest, UnsupportedSystemInfoError)
    {
        EXPECT_CALL(*system_info_cache, GetSystemInfo(::testing::Eq(kMockSystemContext), ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, UnsupportedBeforeNavi1)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x8E;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["eRev"]   = 0xFF;

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, UnsupportedNavi1)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x8F;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["eRev"]   = 0x0;

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, UnsupportedFutureFamily)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0xFF;

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, Unsupported2210)
    {
        system_info_json["system"]["driver"]["packagingVersion"] = "22.10-220717n-230356E-ATI";

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, UnsupportedLowerMajorVersion)
    {
        system_info_json["system"]["driver"]["packagingVersion"] = "21.100-220717n-230356E-ATI";

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, SupportedFutureMajorVersion)
    {
        system_info_json["system"]["driver"]["packagingVersion"] = "100.0-220717n-230356E-ATI";

        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, SupportedNavi2MinRevision)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x8F;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["eRev"]   = 0x28;

        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, SupportedNavi3)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x91;

        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, SupportedRembrandt)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x92;

        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kIdle);
    }

    TEST_F(RraTraceSourceTest, SupportedPhoenix)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x94;

        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, SupportedGpuFamily)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x9A;

        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, SupportedUnsupportedMGPU)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x8F;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["eRev"]   = 0x28;
        system_info_json["system"]["gpus"][1]["asic"]["ids"]["family"] = 0xFF;

        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, UnsupportedSupportedMGPU)
    {
        system_info_json["system"]["gpus"][1]["asic"]["ids"]["family"] = 0x8F;
        system_info_json["system"]["gpus"][1]["asic"]["ids"]["eRev"]   = 0x28;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0xFF;

        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, UnsupportedUnsupportedMGPU)
    {
        system_info_json["system"]["gpus"][1]["asic"]["ids"]["family"] = 0xFF;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0xFF;

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, SupportedSupportedMGPU)
    {
        system_info_json["system"]["gpus"][1]["asic"]["ids"]["family"] = 0x8F;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x8F;

        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
    }

    // Abort tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RraTraceSourceTest, AbortIsUnsupported)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        EXPECT_EQ(Result::kUnsupported, source->RequestAbortTrace());
    }

    // Take trace tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RraTraceSourceTest, BeginFailsIfDisconnected)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD HIP Driver", DD_API_INVALID_HANDLE);

        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kFailure, source->RequestBeginTrace());
    }

    TEST_F(RraTraceSourceTest, BeginFailsIfDisabled)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update("AMD HIP Driver", kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kFailure, source->RequestBeginTrace());
    }

    TEST_F(RraTraceSourceTest, BeginFailsIfApiCallFails)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        bool callback_called = false;
        EXPECT_CALL(rra_api_mock, RequestTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        auto source = MockSource();
        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kError, status);
            EXPECT_EQ("", path);
            callback_called = true;
        });

        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kFailure, source->RequestBeginTrace());
        EXPECT_TRUE(callback_called);
    }

    TEST_F(RraTraceSourceTest, BeginSucceeds)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        EXPECT_CALL(rra_api_mock, RequestTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        EXPECT_CALL(rra_api_mock, CollectTrace(kMockClientContext, 1000, ::testing::Ne(nullptr))).WillRepeatedly(::testing::Return(DD_RESULT_SUCCESS));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->RequestBeginTrace());
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, WaitsForTraceToBeReadyToCollect)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        EXPECT_CALL(rra_api_mock, RequestTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        EXPECT_CALL(rra_api_mock, CollectTrace(kMockClientContext, 1000, ::testing::Ne(nullptr)))
            .WillRepeatedly(::testing::Return(DD_RESULT_COMMON_UNSUPPORTED));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->RequestBeginTrace());
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        EXPECT_TRUE(WaitForCondition([&]() { return source->GetStatus().GetStage() == TraceSourceStage::kCapturing; }));
    }

    TEST_F(RraTraceSourceTest, WaitsIfCollectHasAnError)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        EXPECT_CALL(rra_api_mock, RequestTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        EXPECT_CALL(rra_api_mock, CollectTrace(kMockClientContext, 1000, ::testing::Ne(nullptr))).WillRepeatedly(::testing::Return(DD_RESULT_UNKNOWN));

        bool callback_called = false;
        auto source          = MockSource();
        source->SetTraceCompletedCallback([&](TraceCompletionStatus, const std::string&) { callback_called = true; });

        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->RequestBeginTrace());
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
        EXPECT_FALSE(callback_called);
    }

    TEST_F(RraTraceSourceTest, CallsCompletedCallbackIfErrorAndPath)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        bool callback_called = false;
        auto source          = MockSource();

        EXPECT_CALL(rra_api_mock, RequestTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        EXPECT_CALL(rra_api_mock, CollectTrace(kMockClientContext, 1000, ::testing::Ne(nullptr)))
            .WillRepeatedly([&](DDModuleClientContext, size_t, const DDByteWriter* writer) {
                size_t    num_bytes = 100;
                DD_RESULT result    = writer->pfnBegin(writer->pUserdata, &num_bytes);
                EXPECT_EQ(TraceSourceStage::kDumping, source->GetStatus().GetStage());
                EXPECT_EQ(num_bytes, source->GetStatus().total_bytes_to_dump);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                EXPECT_EQ(source->GetStatus().stage_progress, 0.0);
                EXPECT_EQ(source->GetStatus().num_bytes_dumped, 0);

                return DD_RESULT_UNKNOWN;
            });

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kError, status);
            EXPECT_EQ(stream_provider->GetPath(), path);
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
            EXPECT_EQ(0, stream_provider->GetData().size());
            callback_called = true;
        });

        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->RequestBeginTrace());
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        EXPECT_TRUE(WaitForCondition([&]() { return callback_called; }));
    }

    TEST_F(RraTraceSourceTest, CollectFailsIfCannotGetStream)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        bool callback_called = false;
        auto source          = MockSource();

        EXPECT_CALL(rra_api_mock, RequestTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        EXPECT_CALL(rra_api_mock, CollectTrace(kMockClientContext, 1000, ::testing::Ne(nullptr)))
            .WillRepeatedly([&](DDModuleClientContext, size_t, const DDByteWriter* writer) {
                stream_provider->SetNextStreamsIsNullptr();

                size_t    num_bytes = 100;
                DD_RESULT result    = writer->pfnBegin(writer->pUserdata, &num_bytes);
                EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
                EXPECT_EQ(DD_RESULT_UNKNOWN, result);

                EXPECT_EQ(source->GetStatus().stage_progress, 0.0);
                EXPECT_EQ(source->GetStatus().num_bytes_dumped, 0);

                return result;
            });

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kError, status);
            EXPECT_EQ(stream_provider->GetPath(), path);
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
            EXPECT_EQ(0, stream_provider->GetData().size());
            callback_called = true;
        });

        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->RequestBeginTrace());
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        EXPECT_TRUE(WaitForCondition([&]() { return callback_called; }));
    }

    TEST_F(RraTraceSourceTest, CollectFailsIfBadStream)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        bool callback_called = false;
        auto source          = MockSource();

        EXPECT_CALL(rra_api_mock, RequestTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        EXPECT_CALL(rra_api_mock, CollectTrace(kMockClientContext, 1000, ::testing::Ne(nullptr)))
            .WillRepeatedly([&](DDModuleClientContext, size_t, const DDByteWriter* writer) {
                stream_provider->SetNextStreamsIsBad();

                size_t    num_bytes = 100;
                DD_RESULT result    = writer->pfnBegin(writer->pUserdata, &num_bytes);
                EXPECT_EQ(TraceSourceStage::kDumping, source->GetStatus().GetStage());
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                result = writer->pfnWriteBytes(writer->pUserdata, &num_bytes, sizeof(size_t));
                EXPECT_EQ(DD_RESULT_UNKNOWN, result);

                EXPECT_EQ(source->GetStatus().stage_progress, 0.0);
                EXPECT_EQ(source->GetStatus().num_bytes_dumped, 0);

                return result;
            });

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kError, status);
            EXPECT_EQ(stream_provider->GetPath(), path);
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
            EXPECT_EQ(0, stream_provider->GetData().size());
            callback_called = true;
        });

        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->RequestBeginTrace());
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        EXPECT_TRUE(WaitForCondition([&]() { return callback_called; }));
    }

    TEST_F(RraTraceSourceTest, CollectSuceeds)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        bool callback_called = false;
        auto source          = MockSource();

        std::string string_to_write = "hello world";
        EXPECT_CALL(rra_api_mock, RequestTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        EXPECT_CALL(rra_api_mock, CollectTrace(kMockClientContext, 1000, ::testing::Ne(nullptr)))
            .WillRepeatedly([&](DDModuleClientContext, size_t, const DDByteWriter* writer) {
                size_t    total_bytes = string_to_write.length() + 1;
                DD_RESULT result      = writer->pfnBegin(writer->pUserdata, &total_bytes);

                EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDumping);
                EXPECT_EQ(source->GetStatus().stage_progress, 0.0);
                EXPECT_EQ(source->GetStatus().total_bytes_to_dump, total_bytes);
                EXPECT_EQ(source->GetStatus().num_bytes_dumped, 0);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                result = writer->pfnWriteBytes(writer->pUserdata, string_to_write.c_str(), 5);
                EXPECT_FLOAT_EQ(0.41666666f, source->GetStatus().stage_progress);
                EXPECT_EQ(source->GetStatus().total_bytes_to_dump, total_bytes);
                EXPECT_EQ(source->GetStatus().num_bytes_dumped, 5);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                result = writer->pfnWriteBytes(writer->pUserdata, string_to_write.c_str() + 5, total_bytes - 5);
                EXPECT_FLOAT_EQ(1.0, source->GetStatus().stage_progress);
                EXPECT_EQ(source->GetStatus().total_bytes_to_dump, total_bytes);
                EXPECT_EQ(source->GetStatus().num_bytes_dumped, total_bytes);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                writer->pfnEnd(writer->pUserdata, result);
                return result;
            });

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kCompleted, status);
            EXPECT_EQ(stream_provider->GetPath(), path);
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
            EXPECT_EQ(string_to_write.size() + 1, stream_provider->GetData().size());
            EXPECT_STREQ(string_to_write.c_str(), stream_provider->GetData().data());
            callback_called = true;
        });

        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->RequestBeginTrace());
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        EXPECT_TRUE(WaitForCondition([&]() { return callback_called; }));
        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, CollectWaitsForTraceToBeReady)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        bool callback_called = false;
        auto source          = MockSource();

        std::string string_to_write = "hello world";
        int         total_tries     = 0;
        EXPECT_CALL(rra_api_mock, RequestTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        EXPECT_CALL(rra_api_mock, CollectTrace(kMockClientContext, 1000, ::testing::Ne(nullptr)))
            .WillRepeatedly([&](DDModuleClientContext, size_t, const DDByteWriter* writer) {
                if (++total_tries <= 2)
                {
                    return DD_RESULT_COMMON_UNSUPPORTED;
                }

                size_t    total_bytes = string_to_write.length() + 1;
                DD_RESULT result      = writer->pfnBegin(writer->pUserdata, &total_bytes);

                EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDumping);
                EXPECT_EQ(source->GetStatus().stage_progress, 0.0);
                EXPECT_EQ(source->GetStatus().total_bytes_to_dump, total_bytes);
                EXPECT_EQ(source->GetStatus().num_bytes_dumped, 0);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                result = writer->pfnWriteBytes(writer->pUserdata, string_to_write.c_str(), total_bytes);
                EXPECT_FLOAT_EQ(1.0, source->GetStatus().stage_progress);
                EXPECT_EQ(source->GetStatus().total_bytes_to_dump, total_bytes);
                EXPECT_EQ(source->GetStatus().num_bytes_dumped, total_bytes);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                writer->pfnEnd(writer->pUserdata, result);
                return result;
            });

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kCompleted, status);
            EXPECT_EQ(stream_provider->GetPath(), path);
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
            EXPECT_EQ(string_to_write.size() + 1, stream_provider->GetData().size());
            EXPECT_STREQ(string_to_write.c_str(), stream_provider->GetData().data());
            callback_called = true;
        });

        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->RequestBeginTrace());
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        EXPECT_TRUE(WaitForCondition([&]() { return callback_called; }, 1000));
        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, CollectRetriesIfDumpFails)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        auto        source          = MockSource();
        int         total_tries     = 0;
        std::string string_to_write = "hello world";

        EXPECT_CALL(rra_api_mock, RequestTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        EXPECT_CALL(rra_api_mock, CollectTrace(kMockClientContext, 1000, ::testing::Ne(nullptr)))
            .WillRepeatedly([&](DDModuleClientContext, size_t, const DDByteWriter* writer) {
                size_t    total_bytes = string_to_write.length() + 1;
                DD_RESULT result      = writer->pfnBegin(writer->pUserdata, &total_bytes);

                EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDumping);
                EXPECT_EQ(source->GetStatus().stage_progress, 0.0);
                EXPECT_EQ(source->GetStatus().total_bytes_to_dump, total_bytes);
                EXPECT_EQ(source->GetStatus().num_bytes_dumped, 0);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                if (++total_tries <= 2)
                {
                    return DD_RESULT_UNKNOWN;
                }

                result = writer->pfnWriteBytes(writer->pUserdata, string_to_write.c_str(), total_bytes);
                EXPECT_FLOAT_EQ(1.0, source->GetStatus().stage_progress);
                EXPECT_EQ(source->GetStatus().total_bytes_to_dump, total_bytes);
                EXPECT_EQ(source->GetStatus().num_bytes_dumped, total_bytes);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                writer->pfnEnd(writer->pUserdata, result);
                return result;
            });

        int callback_count = 0;
        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(stream_provider->GetPath(), path);
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());

            if (++callback_count <= 2)
            {
                EXPECT_EQ(TraceCompletionStatus::kError, status);
            }
            else
            {
                EXPECT_EQ(string_to_write.size() + 1, stream_provider->GetData().size());
                EXPECT_STREQ(string_to_write.c_str(), stream_provider->GetData().data());
            }
        });

        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->RequestBeginTrace());
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        EXPECT_TRUE(WaitForCondition([&]() { return callback_count == 3; }, 1000));
        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, CollectUpdatesTotalNumberOfBytes)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        bool callback_called = false;
        auto source          = MockSource();

        std::string string_to_write = "hello world";
        EXPECT_CALL(rra_api_mock, RequestTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        EXPECT_CALL(rra_api_mock, CollectTrace(kMockClientContext, 1000, ::testing::Ne(nullptr)))
            .WillRepeatedly([&](DDModuleClientContext, size_t, const DDByteWriter* writer) {
                size_t    total_bytes = 5;
                DD_RESULT result      = writer->pfnBegin(writer->pUserdata, &total_bytes);

                EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kDumping);
                EXPECT_EQ(source->GetStatus().stage_progress, 0.0);
                EXPECT_EQ(source->GetStatus().total_bytes_to_dump, total_bytes);
                EXPECT_EQ(source->GetStatus().num_bytes_dumped, 0);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                result = writer->pfnWriteBytes(writer->pUserdata, string_to_write.c_str(), 5);
                EXPECT_FLOAT_EQ(1.0, source->GetStatus().stage_progress);
                EXPECT_EQ(source->GetStatus().total_bytes_to_dump, total_bytes);
                EXPECT_EQ(source->GetStatus().num_bytes_dumped, 5);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                total_bytes = string_to_write.size() + 1;
                result      = writer->pfnWriteBytes(writer->pUserdata, string_to_write.c_str() + 5, total_bytes - 5);
                EXPECT_FLOAT_EQ(1.0, source->GetStatus().stage_progress);
                EXPECT_EQ(source->GetStatus().total_bytes_to_dump, total_bytes);
                EXPECT_EQ(source->GetStatus().num_bytes_dumped, total_bytes);
                EXPECT_EQ(DD_RESULT_SUCCESS, result);

                writer->pfnEnd(writer->pUserdata, result);
                return result;
            });

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kCompleted, status);
            EXPECT_EQ(stream_provider->GetPath(), path);
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
            EXPECT_EQ(string_to_write.size() + 1, stream_provider->GetData().size());
            EXPECT_STREQ(string_to_write.c_str(), stream_provider->GetData().data());
            callback_called = true;
        });

        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->RequestBeginTrace());
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        EXPECT_TRUE(WaitForCondition([&]() { return callback_called; }));
        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, TransitionsToDisconnectedIfCapturing)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        bool callback_called = false;
        auto source          = MockSource();

        EXPECT_CALL(rra_api_mock, RequestTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_SUCCESS));
        EXPECT_CALL(rra_api_mock, CollectTrace(kMockClientContext, 1000, ::testing::Ne(nullptr)))
            .WillRepeatedly(::testing::Return(DD_RESULT_COMMON_UNSUPPORTED));

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kAborted, status);
            EXPECT_EQ("", path);
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
            callback_called = true;
        });

        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->RequestBeginTrace());
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);

        EXPECT_TRUE(WaitForCondition([&]() { return callback_called; }));
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
    }

    // Prepared delayed capture
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RraTraceSourceTest, MovesToWaitingForCaptureIfIdle)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());

        EXPECT_EQ(Result::kSuccess, source->PrepareForDelayedCapture());
        EXPECT_EQ(TraceSourceStage::kWaitingToBeginCapture, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, DoesntWaitForCaptureIfNotIdle)
    {
        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());

        EXPECT_EQ(Result::kFailure, source->PrepareForDelayedCapture());
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, WillCallRequestCaptureWhenWaiting)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        EXPECT_CALL(rra_api_mock, RequestTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->PrepareForDelayedCapture());
        EXPECT_EQ(TraceSourceStage::kWaitingToBeginCapture, source->GetStatus().GetStage());

        EXPECT_EQ(Result::kFailure, source->RequestBeginTrace());
    }

    TEST_F(RraTraceSourceTest, TransitionsToIdleIfWaitingForCapture)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());

        EXPECT_EQ(Result::kSuccess, source->PrepareForDelayedCapture());
        EXPECT_EQ(TraceSourceStage::kWaitingToBeginCapture, source->GetStatus().GetStage());

        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, ContinuesToWaitForCaptureIfStillConnected)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate(2);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());

        EXPECT_EQ(Result::kSuccess, source->PrepareForDelayedCapture());
        EXPECT_EQ(TraceSourceStage::kWaitingToBeginCapture, source->GetStatus().GetStage());

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kWaitingToBeginCapture, source->GetStatus().GetStage());
    }

    TEST_F(RraTraceSourceTest, GoesToDisconnectedIfWaitingForCaptureOnDisconnect)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);

        bool callback_called = false;
        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kAborted, status);
            EXPECT_EQ("", path);
            callback_called = true;
        });

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());

        EXPECT_EQ(Result::kSuccess, source->PrepareForDelayedCapture());
        EXPECT_EQ(TraceSourceStage::kWaitingToBeginCapture, source->GetStatus().GetStage());

        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
        EXPECT_TRUE(callback_called);
    }

    TEST_F(RraTraceSourceTest, ResetsToIdleIfBeginCaptureFails)
    {
        MockQuerySystemInfo();
        ExpectUberTraceConfigUpdate();
        EXPECT_CALL(rra_api_mock, RequestTrace(kMockClientContext)).Times(1).WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);

        bool callback_called = false;
        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kError, status);
            EXPECT_EQ("", path);
            callback_called = true;
        });

        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());

        EXPECT_EQ(Result::kSuccess, source->PrepareForDelayedCapture());
        EXPECT_EQ(TraceSourceStage::kWaitingToBeginCapture, source->GetStatus().GetStage());

        source->RequestBeginTrace();
        EXPECT_EQ(TraceSourceStage::kIdle, source->GetStatus().GetStage());
        EXPECT_TRUE(callback_called);
    }

}  // namespace devtrace
