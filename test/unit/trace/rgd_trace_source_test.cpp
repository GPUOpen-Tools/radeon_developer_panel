// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Tests for the RGD trace source.

#include <memory>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QRegularExpression>
#include <QString>

#include <GPUDetectiveModule.h>
#include <ddApi.h>
#include <ddModule.h>

#include <rgd_trace_source.h>
#include <source_userdata.h>
#include <trace_source_factory.h>

#include <mock/mock_trace_io.h>
#include <test_wait.h>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "json_utils.h"
#include "mock/mock_module_common.h"
#include "mock/mock_rgd_api.h"
#include "mock/mock_system_info_cache.h"

namespace devtrace
{
    static const char* kDx12DriverName = "AMD DirectX12 Driver";

    // Mocks
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class MockRgdSummaryGenerator : public RgdSummaryGenerator
    {
    public:
        virtual ~MockRgdSummaryGenerator() = default;
        MOCK_METHOD(Result,
                    GenerateSummaries,
                    (const std::string&, bool, bool, std::string&, std::string&, std::string&, std::atomic<bool>&, const RgdSummaryOptions&));
    };

    class MockRgdUserdataMapper : public RgdUserdataMapper
    {
    public:
        virtual ~MockRgdUserdataMapper() = default;
        MOCK_METHOD(bool, Parse, (const void*, size_t, const std::string&, RgdUserdata&));
        MOCK_METHOD(bool, Serialize, (RgdUserdata&, std::string&));
    };

    // Test data
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static const DDModuleDataContext   kMockDataContext   = reinterpret_cast<DDModuleDataContext>(0x1234);
    static const DDModuleClientContext kMockClientContext = reinterpret_cast<DDModuleClientContext>(0x4321);
    static const DDModuleSystemContext kMockSystemContext = reinterpret_cast<DDModuleSystemContext>(0x1234);

    static DDGPUDetectiveApi kMockRgdApi;
    static DDModuleCommonApi kMockCommonApi;

    // Test suite / fixture
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class RgdTraceSourceTest : public ::testing::Test
    {
    protected:
        RgdTraceSourceTest()
        {
            JsonUtils::LoadJsonFile(SYSTEM_INFO_V4_JSON_FILENAME, system_info_json_string);

            // The V4 system info JSON uses an old driver, so it's updated so the default is supported
            system_info_json.Parse(system_info_json_string.c_str());
            system_info_json["system"]["driver"]["packagingVersion"].SetString("23.10-220717n-230356E-ATI", system_info_json.GetAllocator());

            common_api_lock_  = MockCommonApi::UseMock(&kMockCommonApi, &common_api_mock);
            rgd_api_lock_     = MockRgdApi::UseMock(&kMockRgdApi, &rgd_api_mock);
            stream_provider   = std::make_shared<MockReadWriteStreamProvider>();
            system_info_cache = std::make_shared<MockSystemInfoCache>(kMockCommonApi);
            summary_generator = std::make_shared<::testing::StrictMock<MockRgdSummaryGenerator>>();
            userdata_mapper   = std::make_shared<::testing::StrictMock<MockRgdUserdataMapper>>();
        }

        // Utils
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        std::shared_ptr<RgdTraceSource> MockSource()
        {
            return TraceSourceFactory::CreateRgdSource(
                kMockDataContext, kMockSystemContext, &kMockRgdApi, kMockCommonApi, stream_provider, summary_generator, userdata_mapper, system_info_cache);
        }

        void MockRegisterCallbacks(DD_RESULT result = DD_RESULT_SUCCESS)
        {
            EXPECT_CALL(rgd_api_mock, SetupFileWriteCallbacks(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
                .Times(1)
                .WillOnce(::testing::Return(result));
        }

        void MockUserdata(const RgdUserdata& userdata)
        {
            const std::string userdata_magic = "userdata_magic_string";
            EXPECT_CALL(common_api_mock,
                        QueryUserdataNode(kMockDataContext, ::testing::StrEq(kCrashAnalysisUserdataKey), ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
                .Times(1)
                .WillOnce([=](DDModuleDataContext data_context, const char* node_name, void* userdata, PFN_ddReceiveBinary receive_bytes) {
                    DEV_TRACE_UNUSED(data_context)
                    DEV_TRACE_UNUSED(node_name)

                    receive_bytes(userdata, userdata_magic.c_str(), userdata_magic.size() + 1);
                    return DD_RESULT_SUCCESS;
                });

            EXPECT_CALL(*userdata_mapper, Parse(::testing::Ne(nullptr), userdata_magic.size() + 1, ::testing::Eq(""), ::testing::_))
                .Times(1)
                .WillOnce([=](const void* data, size_t size, const std::string& default_output_path, RgdUserdata& out_data) {
                    DEV_TRACE_UNUSED(size)
                    DEV_TRACE_UNUSED(default_output_path)
                    if (userdata_magic == static_cast<const char*>(data))
                    {
                        out_data = userdata;
                        return true;
                    }

                    return false;
                });
        }

        void MockQuerySystemInfo()
        {
            EXPECT_CALL(*system_info_cache, GetSystemInfo(::testing::Eq(kMockSystemContext), ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
                .Times(1)
                .WillOnce([&](DDModuleSystemContext, void* userdata, PFN_ddReceiveText receive_text) {
                    rapidjson::StringBuffer                    buffer;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    system_info_json.Accept(writer);
                    receive_text(userdata, buffer.GetString());

                    return DD_RESULT_SUCCESS;
                });
        }

        // IMPORTANT: Strict mocks will make sure that any mock warning is treated as an error.
        // This avoids issues where a test might be erroneously passing.
        ::testing::StrictMock<MockCommonApiDelegate> common_api_mock;
        ::testing::StrictMock<MockRgdApiDelegate>    rgd_api_mock;
        std::shared_ptr<MockSystemInfoCache>         system_info_cache;

        std::shared_ptr<MockReadWriteStreamProvider>                    stream_provider;
        std::shared_ptr<::testing::StrictMock<MockRgdSummaryGenerator>> summary_generator;
        std::shared_ptr<::testing::StrictMock<MockRgdUserdataMapper>>   userdata_mapper;

        rapidjson::Document system_info_json;

    private:
        std::unique_lock<std::mutex> common_api_lock_;
        std::unique_lock<std::mutex> rgd_api_lock_;

        static std::string system_info_json_string;
    };

    std::string RgdTraceSourceTest::system_info_json_string;

    // Factory tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RgdTraceSourceTest, FactoryCreatesNullWithBadDataContext)
    {
        auto factory_result = TraceSourceFactory::CreateRgdSource(
            DD_API_INVALID_HANDLE, kMockSystemContext, &kMockRgdApi, kMockCommonApi, stream_provider, summary_generator, userdata_mapper, system_info_cache);
        EXPECT_EQ(factory_result, nullptr);
    }

    TEST_F(RgdTraceSourceTest, FactoryCreatesNullWithBadSystemContext)
    {
        auto factory_result = TraceSourceFactory::CreateRgdSource(
            kMockDataContext, DD_API_INVALID_HANDLE, &kMockRgdApi, kMockCommonApi, stream_provider, summary_generator, userdata_mapper, system_info_cache);
        EXPECT_EQ(factory_result, nullptr);
    }

    TEST_F(RgdTraceSourceTest, FactoryCreatesNullWithBadApi)
    {
        auto factory_result = TraceSourceFactory::CreateRgdSource(
            kMockDataContext, kMockSystemContext, nullptr, kMockCommonApi, stream_provider, summary_generator, userdata_mapper, system_info_cache);
        EXPECT_EQ(factory_result, nullptr);
    }

    TEST_F(RgdTraceSourceTest, FactoryCreatesNullWithBadStreamProvider)
    {
        auto factory_result = TraceSourceFactory::CreateRgdSource(
            kMockDataContext, kMockSystemContext, &kMockRgdApi, kMockCommonApi, nullptr, summary_generator, userdata_mapper, system_info_cache);
        EXPECT_EQ(factory_result, nullptr);
    }

    TEST_F(RgdTraceSourceTest, FactoryCreatesNullWithBadSummaryGenerator)
    {
        auto factory_result = TraceSourceFactory::CreateRgdSource(
            kMockDataContext, kMockSystemContext, &kMockRgdApi, kMockCommonApi, stream_provider, nullptr, userdata_mapper, system_info_cache);
        EXPECT_EQ(factory_result, nullptr);
    }

    TEST_F(RgdTraceSourceTest, FactoryCreatesNullWithBadUserdataMapper)
    {
        auto factory_result = TraceSourceFactory::CreateRgdSource(
            kMockDataContext, kMockSystemContext, &kMockRgdApi, kMockCommonApi, stream_provider, summary_generator, nullptr, system_info_cache);
        EXPECT_EQ(factory_result, nullptr);
    }

    TEST_F(RgdTraceSourceTest, FactoryCreatesNullWithBadSystemInfoCache)
    {
        auto factory_result = TraceSourceFactory::CreateRgdSource(
            kMockDataContext, kMockSystemContext, &kMockRgdApi, kMockCommonApi, stream_provider, summary_generator, userdata_mapper, nullptr);
        EXPECT_EQ(factory_result, nullptr);
    }

    TEST_F(RgdTraceSourceTest, FactoryCreatesRgdTraceSource)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto factory_result = TraceSourceFactory::CreateRgdSource(
            kMockDataContext, kMockSystemContext, &kMockRgdApi, kMockCommonApi, stream_provider, summary_generator, userdata_mapper, system_info_cache);

        EXPECT_NE(factory_result, nullptr);
    }

    // API tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RgdTraceSourceTest, SelectsDX12Api)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();
        auto source = MockSource();

        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kDirectX12);
    }

    TEST_F(RgdTraceSourceTest, SelectsDX11Api)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();
        auto source = MockSource();

        source->Update("AMD DirectX10/11 Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kDirectX11);
    }

    TEST_F(RgdTraceSourceTest, SelectsDX9Api)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();
        auto source = MockSource();

        source->Update("AMD DirectX9 Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kDirectX9);
    }

    TEST_F(RgdTraceSourceTest, SelectsVulkanApi)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();
        auto source = MockSource();

        source->Update("AMD Vulkan Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kVulkan);
    }

    TEST_F(RgdTraceSourceTest, SelectsOpenClApi)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();
        auto source = MockSource();

        source->Update("AMD OpenCL Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kOpenCl);
    }

    TEST_F(RgdTraceSourceTest, SelectsHipApi)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();
        auto source = MockSource();

        source->Update("AMD HIP Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kHip);
    }

    TEST_F(RgdTraceSourceTest, SelectsOpenGlApi)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();
        auto source = MockSource();

        source->Update("AMD OpenGL Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kOpenGl);
    }

    TEST_F(RgdTraceSourceTest, SelectsUnknownApi)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();
        auto source = MockSource();

        source->Update("AMD DirectX-360 Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(source->GetStatus().current_api, Api::kUnknown);
    }

    TEST_F(RgdTraceSourceTest, CallsStatusUpdateForApiUpdate)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();
        auto source = MockSource();

        bool called_update = false;
        source->SetStatusCallback([&](const TraceSourceStatus& status) {
            DEV_TRACE_UNUSED(status);
            called_update = true;
        });
        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);

        EXPECT_TRUE(called_update);
    }

    TEST_F(RgdTraceSourceTest, DoesntCallStatusUpdateForNoApiChange)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();
        auto source = MockSource();

        bool called_update = false;
        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);
        source->SetStatusCallback([&](const TraceSourceStatus& status) {
            DEV_TRACE_UNUSED(status);
            called_update = true;
        });
        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);

        EXPECT_FALSE(called_update);
    }

    // Status update tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RgdTraceSourceTest, UpdatesStageToDisabledOpenCl)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update("AMD OpenCL Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, UpdatesStageToDisabledHip)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update("AMD HIP Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, UpdatesStageToDisabledVulkan)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update("AMD Vulkan Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, UpdatesStageToErrorIfCallbacksFail)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks(DD_RESULT_UNKNOWN);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kError, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, StaysDisconnectedOpenCl)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update("AMD OpenCL Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, StaysDisconnectedHip)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update("AMD HIP Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, StaysDisconnectedVulkan)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update("AMD Vulkan Driver", DD_API_INVALID_HANDLE);
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, StaysDisconnectedDx)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update(kDx12DriverName, DD_API_INVALID_HANDLE);
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, MovesToCapturingIfCallbacksRegistered)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, UpdatesStageToDisconnectedAfterOpenCl)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update("AMD OpenCL Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());

        source->Update(nullptr, DD_API_INVALID_HANDLE);
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, UpdatesStageToDisconnectedAfterHip)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update("AMD HIP Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());

        source->Update(nullptr, DD_API_INVALID_HANDLE);
        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, StaysDisabledOpenCl)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update("AMD OpenCL Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());

        source->Update("AMD OpenCL Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, StaysDisabledHip)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update("AMD HIP Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());

        source->Update("AMD HIP Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, StaysDisabledVulkan)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update("AMD Vulkan Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());

        source->Update("AMD Vulkan Driver", kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, StaysErrorIfCallbacksNotRegistered)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks(DD_RESULT_UNKNOWN);

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kError, source->GetStatus().GetStage());

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kError, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, StaysCapturingIfCallbacksRegistered)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
    }

    // Unsupported actions
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RgdTraceSourceTest, DumpIsUnsupported)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        EXPECT_EQ(Result::kUnsupported, source->RequestDump());
    }

    TEST_F(RgdTraceSourceTest, AddMarkerIsUnsupported)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        EXPECT_EQ(Result::kUnsupported, source->AddMarker("marker"));
    }

    // Summary Generation
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RgdTraceSourceTest, FailsToQueueSummaryIfUserdataQueryFails)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();
        auto source = MockSource();

        EXPECT_CALL(common_api_mock,
                    QueryUserdataNode(kMockDataContext, ::testing::StrEq(kCrashAnalysisUserdataKey), ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        bool called_callback = false;
        source->SetSummaryFinishedCallback([&](Result result, const std::string& path, bool automatically_queued, const std::string& error_string) {
            DEV_TRACE_UNUSED(result)
            DEV_TRACE_UNUSED(path)
            DEV_TRACE_UNUSED(automatically_queued)
            DEV_TRACE_UNUSED(error_string)

            called_callback = true;
        });

        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kFailure, source->QueueSummaryGeneration("test_path", true, false));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
        EXPECT_FALSE(called_callback);
    }

    TEST_F(RgdTraceSourceTest, DoesntGenerateIfNoSummaries)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source          = MockSource();
        bool called_callback = false;
        source->SetSummaryFinishedCallback([&](Result result, const std::string& path, bool automatically_queued, const std::string& error_string) {
            DEV_TRACE_UNUSED(result);
            DEV_TRACE_UNUSED(path);
            DEV_TRACE_UNUSED(automatically_queued);
            DEV_TRACE_UNUSED(error_string);

            called_callback = true;
        });

        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->QueueSummaryGeneration("test_path", false, false));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
        EXPECT_FALSE(called_callback);
    }

    TEST_F(RgdTraceSourceTest, GeneratesTextSummary)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto              source          = MockSource();
        bool              called_callback = false;
        const std::string dump_path       = "test_path";

        MockUserdata({});
        EXPECT_CALL(*summary_generator,
                    GenerateSummaries(::testing::Eq(dump_path), true, false, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1)
            .WillOnce([&](const std::string&       dump_path_utf8,
                          bool                     generate_text,
                          bool                     generate_json,
                          std::string&             error,
                          std::string&             text_path,
                          std::string&             json_path,
                          std::atomic<bool>&       should_abort,
                          const RgdSummaryOptions& options) {
                DEV_TRACE_UNUSED(dump_path_utf8)
                DEV_TRACE_UNUSED(generate_text)
                DEV_TRACE_UNUSED(generate_json)
                DEV_TRACE_UNUSED(json_path)
                DEV_TRACE_UNUSED(should_abort)

                EXPECT_FALSE(options.expand_markers);
                EXPECT_FALSE(options.show_marker_source);
                EXPECT_EQ(TraceSourceStage::kProcessing, source->GetStatus().GetStage());

                error     = "";
                text_path = dump_path + ".txt";

                return Result::kSuccess;
            });

        source->SetSummaryFinishedCallback([&](Result result, const std::string& path, bool automatically_queued, const std::string& error_string) {
            EXPECT_EQ(Result::kSuccess, result);
            EXPECT_EQ(dump_path + ".txt", path);
            EXPECT_FALSE(automatically_queued);
            EXPECT_TRUE(error_string.empty());

            called_callback = true;
        });

        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->QueueSummaryGeneration(dump_path, true, false));

        EXPECT_TRUE(WaitForCondition([&]() { return called_callback; }));
        EXPECT_TRUE(WaitForCondition([&]() { return source->GetStatus().GetStage() == TraceSourceStage::kDisconnected; }));
    }

    TEST_F(RgdTraceSourceTest, GeneratesJsonSummary)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto              source          = MockSource();
        bool              called_callback = false;
        const std::string dump_path       = "test_path";

        MockUserdata({});
        EXPECT_CALL(*summary_generator,
                    GenerateSummaries(::testing::Eq(dump_path), false, true, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1)
            .WillOnce([&](const std::string&       dump_path_utf8,
                          bool                     generate_text,
                          bool                     generate_json,
                          std::string&             error,
                          std::string&             text_path,
                          std::string&             json_path,
                          std::atomic<bool>&       should_abort,
                          const RgdSummaryOptions& options) {
                DEV_TRACE_UNUSED(dump_path_utf8)
                DEV_TRACE_UNUSED(generate_text)
                DEV_TRACE_UNUSED(generate_json)
                DEV_TRACE_UNUSED(text_path)
                DEV_TRACE_UNUSED(should_abort)

                EXPECT_FALSE(options.expand_markers);
                EXPECT_FALSE(options.show_marker_source);
                EXPECT_EQ(TraceSourceStage::kProcessing, source->GetStatus().GetStage());

                error     = "";
                json_path = dump_path + ".json";

                return Result::kSuccess;
            });

        source->SetSummaryFinishedCallback([&](Result result, const std::string& path, bool automatically_queued, const std::string& error_string) {
            EXPECT_EQ(Result::kSuccess, result);
            EXPECT_EQ(dump_path + ".json", path);
            EXPECT_FALSE(automatically_queued);
            EXPECT_TRUE(error_string.empty());

            called_callback = true;
        });

        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->QueueSummaryGeneration(dump_path, false, true));

        EXPECT_TRUE(WaitForCondition([&]() { return called_callback; }));
        EXPECT_TRUE(WaitForCondition([&]() { return source->GetStatus().GetStage() == TraceSourceStage::kDisconnected; }));
    }

    TEST_F(RgdTraceSourceTest, GeneratesBothSummaries)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto              source         = MockSource();
        uint8_t           callback_count = 0;
        const std::string dump_path      = "test_path";

        MockUserdata({});
        EXPECT_CALL(*summary_generator,
                    GenerateSummaries(::testing::Eq(dump_path), true, true, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1)
            .WillOnce([&](const std::string&       dump_path_utf8,
                          bool                     generate_text,
                          bool                     generate_json,
                          std::string&             error,
                          std::string&             text_path,
                          std::string&             json_path,
                          std::atomic<bool>&       should_abort,
                          const RgdSummaryOptions& options) {
                DEV_TRACE_UNUSED(dump_path_utf8)
                DEV_TRACE_UNUSED(generate_text)
                DEV_TRACE_UNUSED(generate_json)
                DEV_TRACE_UNUSED(should_abort)

                EXPECT_FALSE(options.expand_markers);
                EXPECT_FALSE(options.show_marker_source);
                EXPECT_EQ(TraceSourceStage::kProcessing, source->GetStatus().GetStage());

                error     = "";
                text_path = dump_path + ".txt";
                json_path = dump_path + ".json";

                return Result::kSuccess;
            });

        source->SetSummaryFinishedCallback([&](Result result, const std::string& path, bool automatically_queued, const std::string& error_string) {
            EXPECT_EQ(Result::kSuccess, result);
            EXPECT_FALSE(automatically_queued);
            EXPECT_TRUE(error_string.empty());
            EXPECT_EQ(dump_path + (callback_count == 0 ? ".txt" : ".json"), path);

            ++callback_count;
        });

        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->QueueSummaryGeneration(dump_path, true, true));

        EXPECT_TRUE(WaitForCondition([&]() { return callback_count == 2; }));
        EXPECT_TRUE(WaitForCondition([&]() { return source->GetStatus().GetStage() == TraceSourceStage::kDisconnected; }));
    }

    TEST_F(RgdTraceSourceTest, ProperlyHandsOverErrors)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto              source                = MockSource();
        uint8_t           callback_count        = 0;
        const std::string dump_path             = "test_path";
        const std::string expected_error_string = "error_string_bad";

        MockUserdata({});
        EXPECT_CALL(*summary_generator,
                    GenerateSummaries(::testing::Eq(dump_path), true, true, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1)
            .WillOnce([&](const std::string&       dump_path_utf8,
                          bool                     generate_text,
                          bool                     generate_json,
                          std::string&             error,
                          std::string&             text_path,
                          std::string&             json_path,
                          std::atomic<bool>&       should_abort,
                          const RgdSummaryOptions& options) {
                DEV_TRACE_UNUSED(dump_path_utf8)
                DEV_TRACE_UNUSED(generate_text)
                DEV_TRACE_UNUSED(generate_json)
                DEV_TRACE_UNUSED(should_abort)

                EXPECT_FALSE(options.expand_markers);
                EXPECT_FALSE(options.show_marker_source);
                EXPECT_EQ(TraceSourceStage::kProcessing, source->GetStatus().GetStage());

                error     = expected_error_string;
                text_path = dump_path + ".txt";
                json_path = dump_path + ".json";

                return Result::kExecutableNotFound;
            });

        source->SetSummaryFinishedCallback([&](Result result, const std::string& path, bool automatically_queued, const std::string& error_string) {
            EXPECT_EQ(Result::kExecutableNotFound, result);
            EXPECT_FALSE(automatically_queued);
            EXPECT_EQ(expected_error_string, error_string);
            EXPECT_EQ(dump_path + (callback_count == 0 ? ".txt" : ".json"), path);

            ++callback_count;
        });

        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->QueueSummaryGeneration(dump_path, true, true));

        EXPECT_TRUE(WaitForCondition([&]() { return callback_count == 2; }));
        EXPECT_TRUE(WaitForCondition([&]() { return source->GetStatus().GetStage() == TraceSourceStage::kDisconnected; }));
    }

    TEST_F(RgdTraceSourceTest, GeneratesSummariesWithOptions)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto              source         = MockSource();
        uint8_t           callback_count = 0;
        const std::string dump_path      = "test_path";

        RgdUserdata userdata{};
        userdata.summary_options.expand_markers     = true;
        userdata.summary_options.show_marker_source = true;
        MockUserdata(userdata);

        EXPECT_CALL(*summary_generator,
                    GenerateSummaries(::testing::Eq(dump_path), true, true, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1)
            .WillOnce([&](const std::string&       dump_path_utf8,
                          bool                     generate_text,
                          bool                     generate_json,
                          std::string&             error,
                          std::string&             text_path,
                          std::string&             json_path,
                          std::atomic<bool>&       should_abort,
                          const RgdSummaryOptions& options) {
                DEV_TRACE_UNUSED(dump_path_utf8)
                DEV_TRACE_UNUSED(generate_text)
                DEV_TRACE_UNUSED(generate_json)
                DEV_TRACE_UNUSED(should_abort)

                EXPECT_TRUE(options.expand_markers);
                EXPECT_TRUE(options.show_marker_source);
                EXPECT_EQ(TraceSourceStage::kProcessing, source->GetStatus().GetStage());

                error     = "";
                text_path = dump_path + ".txt";
                json_path = dump_path + ".json";

                return Result::kSuccess;
            });

        source->SetSummaryFinishedCallback([&](Result result, const std::string& path, bool automatically_queued, const std::string& error_string) {
            EXPECT_EQ(Result::kSuccess, result);
            EXPECT_FALSE(automatically_queued);
            EXPECT_TRUE(error_string.empty());
            EXPECT_EQ(dump_path + (callback_count == 0 ? ".txt" : ".json"), path);

            ++callback_count;
        });

        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->QueueSummaryGeneration(dump_path, true, true));

        EXPECT_TRUE(WaitForCondition([&]() { return callback_count == 2; }));
        EXPECT_TRUE(WaitForCondition([&]() { return source->GetStatus().GetStage() == TraceSourceStage::kDisconnected; }));
    }

    TEST_F(RgdTraceSourceTest, GeneratesSummaryWhileCapturing)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);

        bool              called_callback = false;
        const std::string dump_path       = "test_path";

        MockUserdata({});
        EXPECT_CALL(*summary_generator,
                    GenerateSummaries(::testing::Eq(dump_path), true, false, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1)
            .WillOnce([&](const std::string&       dump_path_utf8,
                          bool                     generate_text,
                          bool                     generate_json,
                          std::string&             error,
                          std::string&             text_path,
                          std::string&             json_path,
                          std::atomic<bool>&       should_abort,
                          const RgdSummaryOptions& options) {
                DEV_TRACE_UNUSED(dump_path_utf8)
                DEV_TRACE_UNUSED(generate_text)
                DEV_TRACE_UNUSED(generate_json)
                DEV_TRACE_UNUSED(json_path)
                DEV_TRACE_UNUSED(should_abort)

                EXPECT_FALSE(options.expand_markers);
                EXPECT_FALSE(options.show_marker_source);
                EXPECT_EQ(TraceSourceStage::kProcessing, source->GetStatus().GetStage());

                error     = "";
                text_path = dump_path + ".txt";

                return Result::kSuccess;
            });

        source->SetSummaryFinishedCallback([&](Result result, const std::string& path, bool automatically_queued, const std::string& error_string) {
            EXPECT_EQ(Result::kSuccess, result);
            EXPECT_EQ(dump_path + ".txt", path);
            EXPECT_FALSE(automatically_queued);
            EXPECT_TRUE(error_string.empty());

            called_callback = true;
        });

        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->QueueSummaryGeneration(dump_path, true, false));

        EXPECT_TRUE(WaitForCondition([&]() { return called_callback; }));
        EXPECT_TRUE(WaitForCondition([&]() { return source->GetStatus().GetStage() == TraceSourceStage::kCapturing; }));
    }

    TEST_F(RgdTraceSourceTest, GeneratesSummaryWhileSystemNotSupported)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x8D;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["eRev"]   = 0x0;

        MockQuerySystemInfo();
        auto source = MockSource();

        bool              called_callback = false;
        const std::string dump_path       = "test_path";

        MockUserdata({});
        EXPECT_CALL(*summary_generator,
                    GenerateSummaries(::testing::Eq(dump_path), true, false, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1)
            .WillOnce([&](const std::string&       dump_path_utf8,
                          bool                     generate_text,
                          bool                     generate_json,
                          std::string&             error,
                          std::string&             text_path,
                          std::string&             json_path,
                          std::atomic<bool>&       should_abort,
                          const RgdSummaryOptions& options) {
                DEV_TRACE_UNUSED(dump_path_utf8)
                DEV_TRACE_UNUSED(generate_text)
                DEV_TRACE_UNUSED(generate_json)
                DEV_TRACE_UNUSED(json_path)
                DEV_TRACE_UNUSED(should_abort)

                EXPECT_FALSE(options.expand_markers);
                EXPECT_FALSE(options.show_marker_source);
                EXPECT_EQ(TraceSourceStage::kProcessing, source->GetStatus().GetStage());

                error     = "";
                text_path = dump_path + ".txt";

                return Result::kSuccess;
            });

        source->SetSummaryFinishedCallback([&](Result result, const std::string& path, bool automatically_queued, const std::string& error_string) {
            EXPECT_EQ(Result::kSuccess, result);
            EXPECT_EQ(dump_path + ".txt", path);
            EXPECT_FALSE(automatically_queued);
            EXPECT_TRUE(error_string.empty());

            called_callback = true;
        });

        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->QueueSummaryGeneration(dump_path, true, false));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        EXPECT_TRUE(WaitForCondition([&]() { return called_callback; }));
        EXPECT_TRUE(WaitForCondition([&]() { return source->GetStatus().GetStage() == TraceSourceStage::kDisabled; }));
    }

    TEST_F(RgdTraceSourceTest, GeneratesSummaryWhileDisabled)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update("AMD HIP Driver", kMockClientContext);

        bool              called_callback = false;
        const std::string dump_path       = "test_path";

        MockUserdata({});
        EXPECT_CALL(*summary_generator,
                    GenerateSummaries(::testing::Eq(dump_path), true, false, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1)
            .WillOnce([&](const std::string&       dump_path_utf8,
                          bool                     generate_text,
                          bool                     generate_json,
                          std::string&             error,
                          std::string&             text_path,
                          std::string&             json_path,
                          std::atomic<bool>&       should_abort,
                          const RgdSummaryOptions& options) {
                DEV_TRACE_UNUSED(dump_path_utf8)
                DEV_TRACE_UNUSED(generate_text)
                DEV_TRACE_UNUSED(generate_json)
                DEV_TRACE_UNUSED(json_path)
                DEV_TRACE_UNUSED(should_abort)

                EXPECT_FALSE(options.expand_markers);
                EXPECT_FALSE(options.show_marker_source);
                EXPECT_EQ(TraceSourceStage::kProcessing, source->GetStatus().GetStage());

                error     = "";
                text_path = dump_path + ".txt";

                return Result::kSuccess;
            });

        source->SetSummaryFinishedCallback([&](Result result, const std::string& path, bool automatically_queued, const std::string& error_string) {
            EXPECT_EQ(Result::kSuccess, result);
            EXPECT_EQ(dump_path + ".txt", path);
            EXPECT_FALSE(automatically_queued);
            EXPECT_TRUE(error_string.empty());

            called_callback = true;
        });

        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->QueueSummaryGeneration(dump_path, true, false));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        EXPECT_TRUE(WaitForCondition([&]() { return called_callback; }));
        EXPECT_TRUE(WaitForCondition([&]() { return source->GetStatus().GetStage() == TraceSourceStage::kDisabled; }));
    }

    TEST_F(RgdTraceSourceTest, DoesNotGenerateSummaryWhileDumping)
    {
        const DDRdfFileWriter* file_callbacks = nullptr;
        const DDIOHeartbeat*   heartbeat      = nullptr;

        MockQuerySystemInfo();
        EXPECT_CALL(rgd_api_mock, SetupFileWriteCallbacks(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([&](DDModuleDataContext data_context, const DDRdfFileWriter* source_file_callbacks, const DDIOHeartbeat* source_heartbeat) {
                DEV_TRACE_UNUSED(data_context)
                file_callbacks = source_file_callbacks;
                heartbeat      = source_heartbeat;
                return DD_RESULT_SUCCESS;
            });

        auto              source          = MockSource();
        bool              called_callback = false;
        const std::string dump_path       = "test_path";

        MockUserdata({});
        EXPECT_CALL(*summary_generator,
                    GenerateSummaries(::testing::Eq(dump_path), true, false, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1)
            .WillOnce([&](const std::string&       dump_path_utf8,
                          bool                     generate_text,
                          bool                     generate_json,
                          std::string&             error,
                          std::string&             text_path,
                          std::string&             json_path,
                          std::atomic<bool>&       should_abort,
                          const RgdSummaryOptions& options) {
                DEV_TRACE_UNUSED(dump_path_utf8)
                DEV_TRACE_UNUSED(generate_text)
                DEV_TRACE_UNUSED(generate_json)
                DEV_TRACE_UNUSED(json_path)
                DEV_TRACE_UNUSED(should_abort)

                EXPECT_FALSE(options.expand_markers);
                EXPECT_FALSE(options.show_marker_source);
                EXPECT_EQ(TraceSourceStage::kProcessing, source->GetStatus().GetStage());

                error     = "";
                text_path = dump_path + ".txt";

                return Result::kSuccess;
            });

        source->SetSummaryFinishedCallback([&](Result result, const std::string& path, bool automatically_queued, const std::string& error_string) {
            EXPECT_EQ(Result::kSuccess, result);
            EXPECT_EQ(dump_path + ".txt", path);
            EXPECT_FALSE(automatically_queued);
            EXPECT_TRUE(error_string.empty());

            called_callback = true;
        });

        source->GetConfig().generate_text_summary = false;
        source->GetConfig().generate_json_summary = false;

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        DD_RESULT result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_BEGIN, 0);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);
        EXPECT_EQ(TraceSourceStage::kDumping, source->GetStatus().GetStage());

        EXPECT_EQ(Result::kSuccess, source->QueueSummaryGeneration(dump_path, true, false));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        EXPECT_FALSE(called_callback);

        result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_END, 0);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);

        EXPECT_TRUE(WaitForCondition([&]() { return called_callback; }));
    }

    TEST_F(RgdTraceSourceTest, DumpingWorksIfGeneratingSummary)
    {
        const DDRdfFileWriter* file_callbacks = nullptr;
        const DDIOHeartbeat*   heartbeat      = nullptr;

        MockQuerySystemInfo();
        EXPECT_CALL(rgd_api_mock, SetupFileWriteCallbacks(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([&](DDModuleDataContext data_context, const DDRdfFileWriter* source_file_callbacks, const DDIOHeartbeat* source_heartbeat) {
                DEV_TRACE_UNUSED(data_context)
                file_callbacks = source_file_callbacks;
                heartbeat      = source_heartbeat;
                return DD_RESULT_SUCCESS;
            });

        auto              source          = MockSource();
        bool              called_callback = false;
        const std::string dump_path       = "test_path";

        MockUserdata({});
        EXPECT_CALL(*summary_generator,
                    GenerateSummaries(::testing::Eq(dump_path), true, false, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1)
            .WillOnce([&](const std::string&       dump_path_utf8,
                          bool                     generate_text,
                          bool                     generate_json,
                          std::string&             error,
                          std::string&             text_path,
                          std::string&             json_path,
                          std::atomic<bool>&       should_abort,
                          const RgdSummaryOptions& options) {
                DEV_TRACE_UNUSED(dump_path_utf8)
                DEV_TRACE_UNUSED(generate_text)
                DEV_TRACE_UNUSED(generate_json)
                DEV_TRACE_UNUSED(json_path)
                DEV_TRACE_UNUSED(should_abort)

                EXPECT_FALSE(options.expand_markers);
                EXPECT_FALSE(options.show_marker_source);
                EXPECT_EQ(TraceSourceStage::kProcessing, source->GetStatus().GetStage());

                error     = "";
                text_path = dump_path + ".txt";

                while (source->GetStatus().GetStage() != TraceSourceStage::kDumping)
                {
                    std::this_thread::yield();
                }

                return Result::kSuccess;
            });

        source->SetSummaryFinishedCallback([&](Result result, const std::string& path, bool automatically_queued, const std::string& error_string) {
            EXPECT_EQ(Result::kSuccess, result);
            EXPECT_EQ(dump_path + ".txt", path);
            EXPECT_FALSE(automatically_queued);
            EXPECT_TRUE(error_string.empty());

            called_callback = true;
        });

        source->GetConfig().generate_text_summary = false;
        source->GetConfig().generate_json_summary = false;

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        EXPECT_EQ(Result::kSuccess, source->QueueSummaryGeneration(dump_path, true, false));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        EXPECT_FALSE(called_callback);
        EXPECT_EQ(DD_RESULT_SUCCESS, heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_BEGIN, 0));
        EXPECT_EQ(TraceSourceStage::kDumping, source->GetStatus().GetStage());

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        EXPECT_EQ(TraceSourceStage::kDumping, source->GetStatus().GetStage());
        EXPECT_EQ(DD_RESULT_SUCCESS, heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_END, 0));
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
    }

    // and also add a test for starting dumping while generating the summary

    // Abort tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RgdTraceSourceTest, CanAbortSummary)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto              source             = MockSource();
        uint8_t           callback_count     = 0;
        const std::string dump_path          = "test_path";
        bool              started_generation = false;

        MockUserdata({});
        EXPECT_CALL(*summary_generator,
                    GenerateSummaries(::testing::Eq(dump_path), true, true, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1)
            .WillOnce([&](const std::string&       dump_path_utf8,
                          bool                     generate_text,
                          bool                     generate_json,
                          std::string&             error,
                          std::string&             text_path,
                          std::string&             json_path,
                          std::atomic<bool>&       should_abort,
                          const RgdSummaryOptions& options) {
                DEV_TRACE_UNUSED(dump_path_utf8)
                DEV_TRACE_UNUSED(generate_text)
                DEV_TRACE_UNUSED(generate_json)

                EXPECT_FALSE(options.expand_markers);
                EXPECT_FALSE(options.show_marker_source);
                EXPECT_EQ(TraceSourceStage::kProcessing, source->GetStatus().GetStage());

                error     = "";
                text_path = dump_path + ".txt";
                json_path = dump_path + ".json";

                const auto end = std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
                while (std::chrono::system_clock::now() < end)
                {
                    started_generation = true;

                    if (should_abort)
                    {
                        return Result::kSuccess;
                    }

                    std::this_thread::yield();
                }

                return Result::kFailure;
            });

        source->SetSummaryFinishedCallback([&](Result result, const std::string& path, bool automatically_queued, const std::string& error_string) {
            EXPECT_EQ(Result::kSuccess, result);
            EXPECT_FALSE(automatically_queued);
            EXPECT_TRUE(error_string.empty());
            EXPECT_EQ(dump_path + (callback_count == 0 ? ".txt" : ".json"), path);

            ++callback_count;
        });

        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->QueueSummaryGeneration(dump_path, true, true));

        EXPECT_TRUE(WaitForCondition([&]() { return started_generation; }));
        EXPECT_EQ(Result::kSuccess, source->RequestAbortTrace());

        EXPECT_TRUE(WaitForCondition([&]() { return callback_count == 2; }));
        EXPECT_TRUE(WaitForCondition([&]() { return source->GetStatus().GetStage() == TraceSourceStage::kDisconnected; }));
    }

    TEST_F(RgdTraceSourceTest, WontAbortSummaryIfNotCurrentlyProcessing)
    {
        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto              source         = MockSource();
        uint8_t           callback_count = 0;
        const std::string dump_path      = "test_path";

        MockUserdata({});
        EXPECT_CALL(*summary_generator,
                    GenerateSummaries(::testing::Eq(dump_path), true, true, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1)
            .WillOnce([&](const std::string&       dump_path_utf8,
                          bool                     generate_text,
                          bool                     generate_json,
                          std::string&             error,
                          std::string&             text_path,
                          std::string&             json_path,
                          std::atomic<bool>&       should_abort,
                          const RgdSummaryOptions& options) {
                DEV_TRACE_UNUSED(dump_path_utf8)
                DEV_TRACE_UNUSED(generate_text)
                DEV_TRACE_UNUSED(generate_json)
                DEV_TRACE_UNUSED(should_abort)

                EXPECT_FALSE(options.expand_markers);
                EXPECT_FALSE(options.show_marker_source);
                EXPECT_EQ(TraceSourceStage::kProcessing, source->GetStatus().GetStage());

                if (should_abort)
                {
                    return Result::kFailure;
                }

                error     = "";
                text_path = dump_path + ".txt";
                json_path = dump_path + ".json";

                return Result::kSuccess;
            });

        source->SetSummaryFinishedCallback([&](Result result, const std::string& path, bool automatically_queued, const std::string& error_string) {
            EXPECT_EQ(Result::kSuccess, result);
            EXPECT_FALSE(automatically_queued);
            EXPECT_TRUE(error_string.empty());
            EXPECT_EQ(dump_path + (callback_count == 0 ? ".txt" : ".json"), path);

            ++callback_count;
        });

        EXPECT_EQ(TraceSourceStage::kDisconnected, source->GetStatus().GetStage());
        EXPECT_EQ(Result::kSuccess, source->RequestAbortTrace());
        EXPECT_EQ(Result::kSuccess, source->QueueSummaryGeneration(dump_path, true, true));

        EXPECT_TRUE(WaitForCondition([&]() { return callback_count == 2; }));
        EXPECT_TRUE(WaitForCondition([&]() { return source->GetStatus().GetStage() == TraceSourceStage::kDisconnected; }));
    }

    // Dump tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RgdTraceSourceTest, DumpFailsIfInitialResultIsBad)
    {
        const DDRdfFileWriter* file_callbacks = nullptr;
        const DDIOHeartbeat*   heartbeat      = nullptr;

        MockQuerySystemInfo();
        EXPECT_CALL(rgd_api_mock, SetupFileWriteCallbacks(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([&](DDModuleDataContext data_context, const DDRdfFileWriter* source_file_callbacks, const DDIOHeartbeat* source_heartbeat) {
                DEV_TRACE_UNUSED(data_context)
                file_callbacks = source_file_callbacks;
                heartbeat      = source_heartbeat;
                return DD_RESULT_SUCCESS;
            });

        auto source = MockSource();
        ASSERT_NE(nullptr, file_callbacks);
        ASSERT_NE(nullptr, heartbeat);

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kCapturing);
        EXPECT_EQ(DD_RESULT_UNKNOWN, heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_UNKNOWN, DD_IO_STATUS_BEGIN, 0));
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kCapturing);
    }

    TEST_F(RgdTraceSourceTest, DumpFailsIfProvderGivesNullStream)
    {
        stream_provider->SetNextStreamsIsNullptr();
        const DDRdfFileWriter* file_callbacks = nullptr;
        const DDIOHeartbeat*   heartbeat      = nullptr;

        MockQuerySystemInfo();
        EXPECT_CALL(rgd_api_mock, SetupFileWriteCallbacks(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([&](DDModuleDataContext data_context, const DDRdfFileWriter* source_file_callbacks, const DDIOHeartbeat* source_heartbeat) {
                DEV_TRACE_UNUSED(data_context)
                file_callbacks = source_file_callbacks;
                heartbeat      = source_heartbeat;
                return DD_RESULT_SUCCESS;
            });

        auto source = MockSource();

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kError, status);

            EXPECT_EQ(stream_provider->GetPath(), path);
            EXPECT_EQ(0, stream_provider->GetData().size());
            EXPECT_TRUE(stream_provider->GetIsStreamClosed());
        });

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
        EXPECT_EQ(DD_RESULT_UNKNOWN, heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_BEGIN, 123));
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, DumpFailsWhenWriteFails)
    {
        stream_provider->SetNextStreamsIsBad();
        const DDRdfFileWriter* file_callbacks = nullptr;
        const DDIOHeartbeat*   heartbeat      = nullptr;

        MockQuerySystemInfo();
        EXPECT_CALL(rgd_api_mock, SetupFileWriteCallbacks(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([&](DDModuleDataContext data_context, const DDRdfFileWriter* source_file_callbacks, const DDIOHeartbeat* source_heartbeat) {
                DEV_TRACE_UNUSED(data_context)
                file_callbacks = source_file_callbacks;
                heartbeat      = source_heartbeat;
                return DD_RESULT_SUCCESS;
            });

        auto        source          = MockSource();
        std::string string_to_write = "hello world";

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kError, status);
            EXPECT_EQ(stream_provider->GetPath(), path);
        });

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

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
        EXPECT_EQ(1, file_callbacks->pfnFileWrite(file_callbacks->pUserData, 5, string_to_write.c_str(), &bytes_written));
        EXPECT_EQ(0, bytes_written);

        result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_UNKNOWN, DD_IO_STATUS_END, 0);
        EXPECT_EQ(DD_RESULT_UNKNOWN, result);

        EXPECT_EQ(0, stream_provider->GetData().size());
        EXPECT_TRUE(stream_provider->GetIsStreamClosed());

        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, DumpsSuccessfully)
    {
        const DDRdfFileWriter* file_callbacks = nullptr;
        const DDIOHeartbeat*   heartbeat      = nullptr;

        MockQuerySystemInfo();
        EXPECT_CALL(rgd_api_mock, SetupFileWriteCallbacks(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([&](DDModuleDataContext data_context, const DDRdfFileWriter* source_file_callbacks, const DDIOHeartbeat* source_heartbeat) {
                DEV_TRACE_UNUSED(data_context)
                file_callbacks = source_file_callbacks;
                heartbeat      = source_heartbeat;
                return DD_RESULT_SUCCESS;
            });

        auto source                               = MockSource();
        source->GetConfig().generate_text_summary = false;
        source->GetConfig().generate_json_summary = false;

        std::string string_to_write = "hello world";

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kCompleted, status);
            EXPECT_EQ(stream_provider->GetPath(), path);
        });

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

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
        EXPECT_EQ(0, file_callbacks->pfnFileWrite(file_callbacks->pUserData, 5, string_to_write.c_str(), &bytes_written));
        EXPECT_EQ(5, bytes_written);

        result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_WRITE, write_length - 5);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);

        EXPECT_FLOAT_EQ(1.0f, source->GetStatus().stage_progress);
        EXPECT_FLOAT_EQ(write_length, source->GetStatus().num_bytes_dumped);

        bytes_written = 0;
        EXPECT_EQ(0, file_callbacks->pfnFileWrite(file_callbacks->pUserData, write_length - 5, string_to_write.c_str() + 5, &bytes_written));
        EXPECT_EQ(write_length - 5, bytes_written);

        result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_END, 0);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kCapturing);

        EXPECT_EQ(write_length, stream_provider->GetData().size());
        EXPECT_STREQ(string_to_write.c_str(), stream_provider->GetData().data());
        EXPECT_TRUE(stream_provider->GetIsStreamClosed());
    }

    TEST_F(RgdTraceSourceTest, WritesEvenIfInitialByteEstimateIsWrong)
    {
        const DDRdfFileWriter* file_callbacks = nullptr;
        const DDIOHeartbeat*   heartbeat      = nullptr;

        MockQuerySystemInfo();
        EXPECT_CALL(rgd_api_mock, SetupFileWriteCallbacks(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([&](DDModuleDataContext data_context, const DDRdfFileWriter* source_file_callbacks, const DDIOHeartbeat* source_heartbeat) {
                DEV_TRACE_UNUSED(data_context)
                file_callbacks = source_file_callbacks;
                heartbeat      = source_heartbeat;
                return DD_RESULT_SUCCESS;
            });

        auto source                               = MockSource();
        source->GetConfig().generate_text_summary = false;
        source->GetConfig().generate_json_summary = false;

        std::string string_to_write = "hello world";

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kCompleted, status);
            EXPECT_EQ(stream_provider->GetPath(), path);
        });

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        size_t    write_length = string_to_write.length() + 1;
        DD_RESULT result       = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_BEGIN, 5);
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
        EXPECT_EQ(0, file_callbacks->pfnFileWrite(file_callbacks->pUserData, 5, string_to_write.c_str(), &bytes_written));
        EXPECT_EQ(5, bytes_written);

        result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_WRITE, write_length - 5);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);

        EXPECT_EQ(write_length, source->GetStatus().total_bytes_to_dump);
        EXPECT_FLOAT_EQ(1.0, source->GetStatus().stage_progress);
        EXPECT_FLOAT_EQ(write_length, source->GetStatus().num_bytes_dumped);

        bytes_written = 0;
        EXPECT_EQ(0, file_callbacks->pfnFileWrite(file_callbacks->pUserData, write_length - 5, string_to_write.c_str() + 5, &bytes_written));
        EXPECT_EQ(write_length - 5, bytes_written);

        result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_END, 0);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kCapturing);

        EXPECT_EQ(write_length, stream_provider->GetData().size());
        EXPECT_STREQ(string_to_write.c_str(), stream_provider->GetData().data());
        EXPECT_TRUE(stream_provider->GetIsStreamClosed());
    }

    TEST_F(RgdTraceSourceTest, ReadCallbacksAreWiredProperly)
    {
        const DDRdfFileWriter* file_callbacks = nullptr;
        const DDIOHeartbeat*   heartbeat      = nullptr;

        MockQuerySystemInfo();
        EXPECT_CALL(rgd_api_mock, SetupFileWriteCallbacks(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([&](DDModuleDataContext data_context, const DDRdfFileWriter* source_file_callbacks, const DDIOHeartbeat* source_heartbeat) {
                DEV_TRACE_UNUSED(data_context)
                file_callbacks = source_file_callbacks;
                heartbeat      = source_heartbeat;
                return DD_RESULT_SUCCESS;
            });

        auto source                               = MockSource();
        source->GetConfig().generate_text_summary = false;
        source->GetConfig().generate_json_summary = false;

        std::string string_to_write = "hello world";

        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kCompleted, status);
            EXPECT_EQ(stream_provider->GetPath(), path);
        });

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        size_t    write_length = string_to_write.length() + 1;
        DD_RESULT result       = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_BEGIN, write_length);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);
        EXPECT_EQ(TraceSourceStage::kDumping, source->GetStatus().GetStage());

        result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_WRITE, write_length);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);

        std::int64_t bytes_written = 0;
        EXPECT_EQ(0, file_callbacks->pfnFileWrite(file_callbacks->pUserData, write_length, string_to_write.c_str(), &bytes_written));
        EXPECT_EQ(write_length, bytes_written);

        std::int64_t value;
        EXPECT_EQ(0, file_callbacks->pfnFileGetSize(file_callbacks->pUserData, &value));
        EXPECT_EQ(write_length, value);

        value = 0;
        EXPECT_EQ(0, file_callbacks->pfnFileTell(file_callbacks->pUserData, &value));
        EXPECT_EQ(write_length, value);

        EXPECT_EQ(0, file_callbacks->pfnFileSeek(file_callbacks->pUserData, 0));

        std::array<char, 12> read_buffer;
        EXPECT_EQ(0, file_callbacks->pfnFileRead(file_callbacks->pUserData, write_length, read_buffer.data(), &value));
        EXPECT_EQ(string_to_write, std::string(read_buffer.data()));
        EXPECT_EQ(write_length, value);

        result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_END, 0);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kCapturing);

        EXPECT_EQ(write_length, stream_provider->GetData().size());
        EXPECT_STREQ(string_to_write.c_str(), stream_provider->GetData().data());
        EXPECT_TRUE(stream_provider->GetIsStreamClosed());
    }

    // Automatic Generation
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RgdTraceSourceTest, SkipsSummaryGenerationIfTraceFails)
    {
        const DDRdfFileWriter* file_callbacks = nullptr;
        const DDIOHeartbeat*   heartbeat      = nullptr;

        MockQuerySystemInfo();
        EXPECT_CALL(rgd_api_mock, SetupFileWriteCallbacks(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([&](DDModuleDataContext data_context, const DDRdfFileWriter* source_file_callbacks, const DDIOHeartbeat* source_heartbeat) {
                DEV_TRACE_UNUSED(data_context)
                file_callbacks = source_file_callbacks;
                heartbeat      = source_heartbeat;
                return DD_RESULT_SUCCESS;
            });

        auto source                               = MockSource();
        source->GetConfig().generate_text_summary = true;
        source->GetConfig().generate_json_summary = false;

        bool called_callback = false;
        source->SetTraceCompletedCallback([&](TraceCompletionStatus status, const std::string& path) {
            EXPECT_EQ(TraceCompletionStatus::kError, status);
            EXPECT_EQ(stream_provider->GetPath(), path);
            called_callback = true;
        });

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        DD_RESULT result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_BEGIN, 0);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);

        result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_UNKNOWN, DD_IO_STATUS_END, 0);
        EXPECT_EQ(DD_RESULT_UNKNOWN, result);
        EXPECT_TRUE(called_callback);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    TEST_F(RgdTraceSourceTest, AutomaticallyGeneratesTextSummary)
    {
        const DDRdfFileWriter* file_callbacks = nullptr;
        const DDIOHeartbeat*   heartbeat      = nullptr;

        MockQuerySystemInfo();
        EXPECT_CALL(rgd_api_mock, SetupFileWriteCallbacks(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([&](DDModuleDataContext data_context, const DDRdfFileWriter* source_file_callbacks, const DDIOHeartbeat* source_heartbeat) {
                DEV_TRACE_UNUSED(data_context)
                file_callbacks = source_file_callbacks;
                heartbeat      = source_heartbeat;
                return DD_RESULT_SUCCESS;
            });

        auto source                               = MockSource();
        source->GetConfig().generate_text_summary = true;
        source->GetConfig().generate_json_summary = false;

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        DD_RESULT result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_BEGIN, 0);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);

        bool generated_summary = false;
        MockUserdata({});
        EXPECT_CALL(
            *summary_generator,
            GenerateSummaries(::testing::Eq(stream_provider->GetPath()), true, false, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1)
            .WillOnce([&](const std::string&       dump_path_utf8,
                          bool                     generate_text,
                          bool                     generate_json,
                          std::string&             error,
                          std::string&             text_path,
                          std::string&             json_path,
                          std::atomic<bool>&       should_abort,
                          const RgdSummaryOptions& options) {
                DEV_TRACE_UNUSED(dump_path_utf8)
                DEV_TRACE_UNUSED(generate_text)
                DEV_TRACE_UNUSED(generate_json)
                DEV_TRACE_UNUSED(error)
                DEV_TRACE_UNUSED(text_path)
                DEV_TRACE_UNUSED(json_path)
                DEV_TRACE_UNUSED(should_abort)

                EXPECT_FALSE(options.expand_markers);
                EXPECT_FALSE(options.show_marker_source);
                EXPECT_EQ(TraceSourceStage::kProcessing, source->GetStatus().GetStage());

                generated_summary = true;
                return Result::kSuccess;
            });

        result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_END, 0);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);

        EXPECT_TRUE(WaitForCondition([&]() { return generated_summary; }));
    }

    TEST_F(RgdTraceSourceTest, AutomaticallyGeneratesJsonSummary)
    {
        const DDRdfFileWriter* file_callbacks = nullptr;
        const DDIOHeartbeat*   heartbeat      = nullptr;

        MockQuerySystemInfo();
        EXPECT_CALL(rgd_api_mock, SetupFileWriteCallbacks(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([&](DDModuleDataContext data_context, const DDRdfFileWriter* source_file_callbacks, const DDIOHeartbeat* source_heartbeat) {
                DEV_TRACE_UNUSED(data_context)
                file_callbacks = source_file_callbacks;
                heartbeat      = source_heartbeat;
                return DD_RESULT_SUCCESS;
            });

        auto source                               = MockSource();
        source->GetConfig().generate_text_summary = false;
        source->GetConfig().generate_json_summary = true;

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        DD_RESULT result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_BEGIN, 0);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);

        bool generated_summary = false;
        MockUserdata({});
        EXPECT_CALL(
            *summary_generator,
            GenerateSummaries(::testing::Eq(stream_provider->GetPath()), false, true, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1)
            .WillOnce([&](const std::string&       dump_path_utf8,
                          bool                     generate_text,
                          bool                     generate_json,
                          std::string&             error,
                          std::string&             text_path,
                          std::string&             json_path,
                          std::atomic<bool>&       should_abort,
                          const RgdSummaryOptions& options) {
                DEV_TRACE_UNUSED(dump_path_utf8)
                DEV_TRACE_UNUSED(generate_text)
                DEV_TRACE_UNUSED(generate_json)
                DEV_TRACE_UNUSED(error)
                DEV_TRACE_UNUSED(text_path)
                DEV_TRACE_UNUSED(json_path)
                DEV_TRACE_UNUSED(should_abort)

                EXPECT_FALSE(options.expand_markers);
                EXPECT_FALSE(options.show_marker_source);
                EXPECT_EQ(TraceSourceStage::kProcessing, source->GetStatus().GetStage());

                generated_summary = true;
                return Result::kSuccess;
            });

        result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_END, 0);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);

        EXPECT_TRUE(WaitForCondition([&]() { return generated_summary; }));
    }

    TEST_F(RgdTraceSourceTest, AutomaticallyGeneratesBothSummaries)
    {
        const DDRdfFileWriter* file_callbacks = nullptr;
        const DDIOHeartbeat*   heartbeat      = nullptr;

        MockQuerySystemInfo();
        EXPECT_CALL(rgd_api_mock, SetupFileWriteCallbacks(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([&](DDModuleDataContext data_context, const DDRdfFileWriter* source_file_callbacks, const DDIOHeartbeat* source_heartbeat) {
                DEV_TRACE_UNUSED(data_context)
                file_callbacks = source_file_callbacks;
                heartbeat      = source_heartbeat;
                return DD_RESULT_SUCCESS;
            });

        auto source                               = MockSource();
        source->GetConfig().generate_text_summary = true;
        source->GetConfig().generate_json_summary = true;

        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());

        DD_RESULT result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_BEGIN, 0);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);

        bool generated_summaries = false;
        MockUserdata({});
        EXPECT_CALL(
            *summary_generator,
            GenerateSummaries(::testing::Eq(stream_provider->GetPath()), true, true, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1)
            .WillOnce([&](const std::string&       dump_path_utf8,
                          bool                     generate_text,
                          bool                     generate_json,
                          std::string&             error,
                          std::string&             text_path,
                          std::string&             json_path,
                          std::atomic<bool>&       should_abort,
                          const RgdSummaryOptions& options) {
                DEV_TRACE_UNUSED(dump_path_utf8)
                DEV_TRACE_UNUSED(generate_text)
                DEV_TRACE_UNUSED(generate_json)
                DEV_TRACE_UNUSED(error)
                DEV_TRACE_UNUSED(text_path)
                DEV_TRACE_UNUSED(json_path)
                DEV_TRACE_UNUSED(should_abort)

                EXPECT_FALSE(options.expand_markers);
                EXPECT_FALSE(options.show_marker_source);
                EXPECT_EQ(TraceSourceStage::kProcessing, source->GetStatus().GetStage());

                generated_summaries = true;
                return Result::kSuccess;
            });

        result = heartbeat->pfnWriteHeartbeat(heartbeat->pUserdata, DD_RESULT_SUCCESS, DD_IO_STATUS_END, 0);
        EXPECT_EQ(DD_RESULT_SUCCESS, result);

        EXPECT_TRUE(WaitForCondition([&]() { return generated_summaries; }));
    }

    // Hardware support test
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RgdTraceSourceTest, UnsupportedSystemInfoError)
    {
        EXPECT_CALL(*system_info_cache, GetSystemInfo(::testing::Eq(kMockSystemContext), ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, UnsupportedLinux)
    {
        system_info_json["system"]["os"]["name"] = "Ubuntu";

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, UnsupportedBeforeNavi1)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x8E;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["eRev"]   = 0xFF;

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, UnsupportedPolaris)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x82;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["eRev"]   = 0x0;

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, UnsupportedVega)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x8D;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["eRev"]   = 0x0;

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, UnsupportedNavi1)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x8F;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["eRev"]   = 0x0;

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, UnsupportedFutureFamily)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0xFF;

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, Unsupported2240)
    {
        system_info_json["system"]["driver"]["packagingVersion"] = "22.40-220717n-230356E-ATI";

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, UnsupportedLowerMinorVersion)
    {
        system_info_json["system"]["driver"]["packagingVersion"] = "23.00-220717n-230356E-ATI";

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, UnsupportedLowerMajorVersion)
    {
        system_info_json["system"]["driver"]["packagingVersion"] = "22.100-220717n-230356E-ATI";

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, SupportedFutureMajorVersion)
    {
        system_info_json["system"]["driver"]["packagingVersion"] = "100.0-220717n-230356E-ATI";

        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, SupportedNavi2MinRevision)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x8F;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["eRev"]   = 0x28;

        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, SupportedNavi3)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x91;

        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, SupportedRembrandt)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x92;

        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(source->GetStatus().GetStage(), TraceSourceStage::kCapturing);
    }

    TEST_F(RgdTraceSourceTest, SupportedPhoenix)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x94;

        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, SupportedUnsupportedMGPU)
    {
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x8F;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["eRev"]   = 0x28;
        system_info_json["system"]["gpus"][1]["asic"]["ids"]["family"] = 0xFF;

        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, UnsupportedSupportedMGPU)
    {
        system_info_json["system"]["gpus"][1]["asic"]["ids"]["family"] = 0x8F;
        system_info_json["system"]["gpus"][1]["asic"]["ids"]["eRev"]   = 0x28;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0xFF;

        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, UnsupportedUnsupportedMGPU)
    {
        system_info_json["system"]["gpus"][1]["asic"]["ids"]["family"] = 0xFF;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0xFF;

        MockQuerySystemInfo();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kDisabled, source->GetStatus().GetStage());
    }

    TEST_F(RgdTraceSourceTest, SupportedSupportedMGPU)
    {
        system_info_json["system"]["gpus"][1]["asic"]["ids"]["family"] = 0x8F;
        system_info_json["system"]["gpus"][0]["asic"]["ids"]["family"] = 0x8F;

        MockQuerySystemInfo();
        MockRegisterCallbacks();

        auto source = MockSource();
        source->Update(kDx12DriverName, kMockClientContext);
        EXPECT_EQ(TraceSourceStage::kCapturing, source->GetStatus().GetStage());
    }

}  // namespace devtrace
