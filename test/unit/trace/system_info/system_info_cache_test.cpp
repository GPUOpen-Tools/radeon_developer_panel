// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Tests for the system info cache.

#include <chrono>
#include <sstream>
#include <string>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <ddModule.h>

#include <system_info_cache.h>

#include "../mock/mock_module_common.h"

namespace devtrace
{

    // Test data
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static const DDModuleSystemContext kMockSystemContext = reinterpret_cast<DDModuleSystemContext>(0x1234);
    static DDModuleCommonApi           kCommonApi;

    // Test suite / fixture
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class SystemInfoCacheTest : public ::testing::Test
    {
    protected:
        static void SystemInfoCallback(void* userdata, const char* contents)
        {
            SystemInfoCacheTest* test = reinterpret_cast<SystemInfoCacheTest*>(userdata);
            test->json_string         = contents;
        }

        SystemInfoCacheTest()
        {
            common_api_lock_ = std::move(MockCommonApi::UseMock(&kCommonApi, &common_api_mock));
        }

        // IMPORTANT: Strict mocks will make sure that any mock warning is treated as an error.
        // This avoids issues where a test might be erroneously passing.
        ::testing::StrictMock<MockCommonApiDelegate> common_api_mock;

        std::string json_string = "";

    private:
        std::unique_lock<std::mutex> common_api_lock_;
    };

    // Tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(SystemInfoCacheTest, QueriesSystemInfoFirstCall)
    {
        EXPECT_CALL(common_api_mock, QuerySystemInfo(kMockSystemContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([](DDModuleSystemContext, void* userdata, PFN_ddReceiveText callback) {
                callback(userdata, "hello");
                return DD_RESULT_SUCCESS;
            });

        SystemInfoCache cache(kCommonApi);
        EXPECT_EQ(DD_RESULT_SUCCESS, cache.GetSystemInfo(kMockSystemContext, this, &SystemInfoCallback));
        EXPECT_STREQ("hello", json_string.c_str());
    }

    TEST_F(SystemInfoCacheTest, UsesCachedValue)
    {
        EXPECT_CALL(common_api_mock, QuerySystemInfo(kMockSystemContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([](DDModuleSystemContext, void* userdata, PFN_ddReceiveText callback) {
                callback(userdata, "hello");
                return DD_RESULT_SUCCESS;
            });

        SystemInfoCache cache(kCommonApi);

        EXPECT_EQ(DD_RESULT_SUCCESS, cache.GetSystemInfo(kMockSystemContext, this, &SystemInfoCallback));
        EXPECT_STREQ("hello", json_string.c_str());
        json_string = "";

        EXPECT_EQ(DD_RESULT_SUCCESS, cache.GetSystemInfo(kMockSystemContext, this, &SystemInfoCallback));
        EXPECT_STREQ("hello", json_string.c_str());
        json_string = "";

        EXPECT_EQ(DD_RESULT_SUCCESS, cache.GetSystemInfo(kMockSystemContext, this, &SystemInfoCallback));
        EXPECT_STREQ("hello", json_string.c_str());
    }

    TEST_F(SystemInfoCacheTest, RefreshesCacheWhenExpired)
    {
        int call_index = 0;
        EXPECT_CALL(common_api_mock, QuerySystemInfo(kMockSystemContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(2)
            .WillRepeatedly([&](DDModuleSystemContext, void* userdata, PFN_ddReceiveText callback) {
                std::ostringstream stream;
                stream << "hello_";
                stream << call_index++;

                std::string result = stream.str();
                callback(userdata, result.c_str());

                return DD_RESULT_SUCCESS;
            });

        SystemInfoCache cache(kCommonApi);

        EXPECT_EQ(DD_RESULT_SUCCESS, cache.GetSystemInfo(kMockSystemContext, this, &SystemInfoCallback));
        EXPECT_STREQ("hello_0", json_string.c_str());
        json_string = "";

        EXPECT_EQ(DD_RESULT_SUCCESS, cache.GetSystemInfo(kMockSystemContext, this, &SystemInfoCallback));
        EXPECT_STREQ("hello_0", json_string.c_str());
        json_string = "";

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        EXPECT_EQ(DD_RESULT_SUCCESS, cache.GetSystemInfo(kMockSystemContext, this, &SystemInfoCallback));
        EXPECT_STREQ("hello_1", json_string.c_str());
        json_string = "";

        EXPECT_EQ(DD_RESULT_SUCCESS, cache.GetSystemInfo(kMockSystemContext, this, &SystemInfoCallback));
        EXPECT_STREQ("hello_1", json_string.c_str());
        json_string = "";
    }

    TEST_F(SystemInfoCacheTest, FailsWhenQueryFails)
    {
        EXPECT_CALL(common_api_mock, QuerySystemInfo(kMockSystemContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        SystemInfoCache cache(kCommonApi);
        EXPECT_EQ(DD_RESULT_UNKNOWN, cache.GetSystemInfo(kMockSystemContext, this, &SystemInfoCallback));
    }

};  // namespace devtrace
