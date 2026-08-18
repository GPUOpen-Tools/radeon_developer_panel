// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Tests for the RGP trace source config.

#include <string.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <ProfilingModule.h>
#include <ddApi.h>

#include <dev_trace_common.h>
#include <rgp_trace_source_config.h>

#include "mock/mock_rgp_api.h"

namespace devtrace
{
    // Test data
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static const DDModuleDataContext kMockDataContext = reinterpret_cast<DDModuleDataContext>(0x1234);
    static DDProfilingApi            kMockRgpApi;

    // In the future if the default value of the property description is actually used that will need to be added here
    static DDProfilingPropertyDescription desc(const char* name, const char* desc, ProfilingProperty id)
    {
        DDProfilingPropertyDescription description;
        description.pName        = name;
        description.pDescription = desc;
        description.id           = static_cast<DDProfilingPropertyId>(id);

        return description;
    }

    // These properties are not reflective of what RGP actually uses, they are just convenient test data.
    static const std::array<const DDProfilingPropertyDescription, 11> kPropDescriptions = {
        desc("bool prop", "inst tracing", ProfilingProperty::kEnableInstructionTracing),
        desc("float prop", "api pso hash", ProfilingProperty::kInstructionTracingApiPsoHash),
        desc("string prop", "prep frames", ProfilingProperty::kNumberOfPreparationFrames),
        desc("int8 prop", "inst trace mask", ProfilingProperty::kShaderEngineInstructionTraceMask),
        desc("int16 prop", "enable spm", ProfilingProperty::kEnableSpm),
        desc("int32 prop", "sample freq", ProfilingProperty::kSpmSampleFrequency),
        desc("int64 prop", "spm mem limit", ProfilingProperty::kSpmMemoryLimit),
        desc("uint8 prop", "sqtt mem limit", ProfilingProperty::kSqttMemoryLimit),
        desc("uint16 prop", "trigger mode", ProfilingProperty::kTriggerMode),
        desc("uint32 prop", "trigger begin", ProfilingProperty::kTriggerMarkerBegin),
        desc("uint64 prop", "trigger end", ProfilingProperty::kTriggerMarkerEnd)};

    // Test suite / fixture
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class RgpTraceSourceConfigTest : public ::testing::Test
    {
    protected:
        RgpTraceSourceConfigTest()
        {
            rgp_api_lock_ = MockRgpApi::UseMock(&kMockRgpApi, &rgp_api_mock);
        }

        // Utils
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        void MockGetPropertyDescriptions()
        {
            EXPECT_CALL(rgp_api_mock, GetPropertyDescriptions(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
                .Times(1)
                .WillOnce([&](DDModuleDataContext data_context, uint32_t* out_num, const DDProfilingPropertyDescription** out_desc) {
                    DEV_TRACE_UNUSED(data_context);
                    *out_num  = static_cast<uint32_t>(kPropDescriptions.size());
                    *out_desc = kPropDescriptions.data();

                    return DD_RESULT_SUCCESS;
                });
        }

        void MockGetPropertyDescriptions(uint8_t property_index)
        {
            ASSERT_LT(property_index, kPropDescriptions.size());
            EXPECT_CALL(rgp_api_mock, GetPropertyDescriptions(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
                .Times(1)
                .WillOnce([=](DDModuleDataContext data_context, uint32_t* out_num, const DDProfilingPropertyDescription** out_desc) {
                    DEV_TRACE_UNUSED(data_context);
                    *out_num  = 1;
                    *out_desc = &kPropDescriptions[property_index];

                    return DD_RESULT_SUCCESS;
                });
        }

        template <typename T>
        void MockGet(const T& value, ProfilingProperty property, DD_PROFILING_PROPERTY_TYPE property_type, uint8_t times = 1)
        {
            EXPECT_CALL(rgp_api_mock, GetPropertyValue(kMockDataContext, ::testing::Eq(static_cast<uint32_t>(property)), ::testing::Ne(nullptr)))
                .Times(times)
                .WillRepeatedly([=](DDModuleDataContext data_context, uint32_t property, DDProfilingPropertyValue* out_value) {
                    DEV_TRACE_UNUSED(data_context);
                    DEV_TRACE_UNUSED(property);

                    DDProfilingPropertyValue prop_value;
                    prop_value.type = property_type;

                    PropertySerializer<T> serializer;
                    EXPECT_TRUE(serializer.Serialize(prop_value, value));

                    *out_value = prop_value;
                    return DD_RESULT_SUCCESS;
                });
        }

        // Template tests
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        template <typename T>
        void TestGetHappyPath(const T& value, ProfilingProperty property, DD_PROFILING_PROPERTY_TYPE property_type)
        {
            MockGetPropertyDescriptions();
            MockGet<T>(value, property, property_type);

            RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
            EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

            T read_value{};
            EXPECT_EQ(Result::kSuccess, config.GetValue<T>(property, read_value));
            EXPECT_EQ(value, read_value);
        }

        template <typename T>
        void TestSetHappyPath(const T& value, ProfilingProperty property, DD_PROFILING_PROPERTY_TYPE property_type)
        {
            MockGetPropertyDescriptions();
            MockGet<T>({}, property, property_type);

            EXPECT_CALL(rgp_api_mock, SetPropertyValue(kMockDataContext, ::testing::Eq(static_cast<uint32_t>(property)), ::testing::Ne(nullptr)))
                .Times(1)
                .WillOnce([&](DDModuleDataContext data_context, uint32_t property, const DDProfilingPropertyValue* out_value) {
                    DEV_TRACE_UNUSED(data_context);
                    DEV_TRACE_UNUSED(property);

                    T                       set_value{};
                    PropertyDeserializer<T> deserializer;
                    EXPECT_TRUE(deserializer.Deserialize(*out_value, set_value));
                    EXPECT_EQ(value, set_value);

                    return DD_RESULT_SUCCESS;
                });

            RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
            EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());
            EXPECT_EQ(Result::kSuccess, config.SetValue<T>(property, value));
        }

        // IMPORTANT: Strict mocks will make sure that any mock warning is treated as an error.
        // This avoids issues where a test might be erroneously passing.
        ::testing::StrictMock<MockRgpApiDelegate> rgp_api_mock;

    private:
        std::unique_lock<std::mutex> rgp_api_lock_;
    };

    // Invalid setup tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RgpTraceSourceConfigTest, TestBadDataContext)
    {
        RgpTraceSourceConfig config(DD_API_INVALID_HANDLE, &kMockRgpApi);
        EXPECT_EQ(Result::kFailure, config.LoadAvailableProperties());

        bool get_result = true;
        EXPECT_EQ(Result::kFailure, config.SetValue<bool>(ProfilingProperty::kEnableInstructionTracing, false));
        EXPECT_EQ(Result::kFailure, config.GetValue<bool>(ProfilingProperty::kEnableInstructionTracing, get_result));

        bool                         compare_result = false;
        RgpTraceSourceConfigSnapshot snapshot;
        EXPECT_EQ(Result::kFailure, config.GetSnapshot(snapshot));
        EXPECT_EQ(Result::kFailure, config.CompareToSnapshot(snapshot, compare_result));
        EXPECT_FALSE(compare_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestBadApi)
    {
        RgpTraceSourceConfig config(kMockDataContext, nullptr);
        EXPECT_EQ(Result::kFailure, config.LoadAvailableProperties());

        bool get_result = true;
        EXPECT_EQ(Result::kFailure, config.SetValue<bool>(ProfilingProperty::kEnableInstructionTracing, false));
        EXPECT_EQ(Result::kFailure, config.GetValue<bool>(ProfilingProperty::kEnableInstructionTracing, get_result));

        bool                         compare_result = false;
        RgpTraceSourceConfigSnapshot snapshot;
        EXPECT_EQ(Result::kFailure, config.GetSnapshot(snapshot));
        EXPECT_EQ(Result::kFailure, config.CompareToSnapshot(snapshot, compare_result));
        EXPECT_FALSE(compare_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestLoadFailure)
    {
        EXPECT_CALL(rgp_api_mock, GetPropertyDescriptions(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kFailure, config.LoadAvailableProperties());
    }

    // Get/Set failure tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RgpTraceSourceConfigTest, TestSetInvalidProperty)
    {
        MockGetPropertyDescriptions();

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());
        EXPECT_EQ(Result::kNotFound, config.SetValue<bool>(static_cast<ProfilingProperty>(-1), true));
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetMismatchType)
    {
        MockGetPropertyDescriptions();
        MockGet<uint64_t>(251231123112311, ProfilingProperty::kTriggerMarkerEnd, DD_PROFILING_PROPERTY_TYPE_UINT64);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());
        EXPECT_EQ(Result::kFailure, config.SetValue<bool>(ProfilingProperty::kTriggerMarkerEnd, true));
    }

    TEST_F(RgpTraceSourceConfigTest, TestGetInvalidProperty)
    {
        MockGetPropertyDescriptions();

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool read_value = false;
        EXPECT_EQ(Result::kNotFound, config.GetValue<bool>(static_cast<ProfilingProperty>(-1), read_value));
    }

    TEST_F(RgpTraceSourceConfigTest, TestGetMismatchType)
    {
        MockGetPropertyDescriptions();
        MockGet<uint64_t>(251231123112311, ProfilingProperty::kTriggerMarkerEnd, DD_PROFILING_PROPERTY_TYPE_UINT64);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool read_value = false;
        EXPECT_EQ(Result::kFailure, config.GetValue<bool>(ProfilingProperty::kTriggerMarkerEnd, read_value));
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetWhenGetApiCallFails)
    {
        MockGetPropertyDescriptions();

        uint32_t property = static_cast<uint32_t>(ProfilingProperty::kEnableInstructionTracing);
        EXPECT_CALL(rgp_api_mock, GetPropertyValue(kMockDataContext, ::testing::Eq(property), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());
        EXPECT_EQ(Result::kFailure, config.SetValue<bool>(ProfilingProperty::kEnableInstructionTracing, false));
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetWhenSetApiCallFails)
    {
        MockGetPropertyDescriptions();
        MockGet(true, ProfilingProperty::kEnableInstructionTracing, DD_PROFILING_PROPERTY_TYPE_BOOL);

        uint32_t property = static_cast<uint32_t>(ProfilingProperty::kEnableInstructionTracing);
        EXPECT_CALL(rgp_api_mock, SetPropertyValue(kMockDataContext, ::testing::Eq(property), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());
        EXPECT_EQ(Result::kFailure, config.SetValue<bool>(ProfilingProperty::kEnableInstructionTracing, false));
    }

    TEST_F(RgpTraceSourceConfigTest, TestGetApiCallFails)
    {
        MockGetPropertyDescriptions();

        uint32_t property = static_cast<uint32_t>(ProfilingProperty::kEnableInstructionTracing);
        EXPECT_CALL(rgp_api_mock, GetPropertyValue(kMockDataContext, ::testing::Eq(property), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool read_value = false;
        EXPECT_EQ(Result::kFailure, config.GetValue<bool>(ProfilingProperty::kEnableInstructionTracing, read_value));
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetFailsWithAStringThats256Chars)
    {
        MockGetPropertyDescriptions();
        MockGet<std::string>("Hello", ProfilingProperty::kNumberOfPreparationFrames, DD_PROFILING_PROPERTY_TYPE_STRING);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        std::string long_string = std::string(256, 'a');
        EXPECT_EQ(Result::kFailure, config.SetValue<std::string>(ProfilingProperty::kNumberOfPreparationFrames, long_string));
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetFailsWithAStringThatsLonger256Chars)
    {
        MockGetPropertyDescriptions();
        MockGet<std::string>("Hello", ProfilingProperty::kNumberOfPreparationFrames, DD_PROFILING_PROPERTY_TYPE_STRING);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        std::string long_string = std::string(512, 'a');
        EXPECT_EQ(Result::kFailure, config.SetValue<std::string>(ProfilingProperty::kNumberOfPreparationFrames, long_string));
    }

    TEST_F(RgpTraceSourceConfigTest, TestGetFailsWithUnknownType)
    {
        MockGetPropertyDescriptions();
        MockGet<std::string>("Hello", ProfilingProperty::kNumberOfPreparationFrames, DD_PROFILING_PROPERTY_TYPE_STRING);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        const char* read_value = nullptr;
        EXPECT_EQ(Result::kFailure, config.GetValue<const char*>(ProfilingProperty::kNumberOfPreparationFrames, read_value));
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetFailsWithUnknownType)
    {
        MockGetPropertyDescriptions();
        MockGet<std::string>("Hello", ProfilingProperty::kNumberOfPreparationFrames, DD_PROFILING_PROPERTY_TYPE_STRING);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());
        EXPECT_EQ(Result::kFailure, config.SetValue<const char*>(ProfilingProperty::kNumberOfPreparationFrames, "hello"));
    }

    // Set happy path tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RgpTraceSourceConfigTest, TestSetBool)
    {
        TestSetHappyPath<bool>(true, ProfilingProperty::kEnableInstructionTracing, DD_PROFILING_PROPERTY_TYPE_BOOL);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetFloat)
    {
        TestSetHappyPath<float>(124.0123f, ProfilingProperty::kInstructionTracingApiPsoHash, DD_PROFILING_PROPERTY_TYPE_FLOAT);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetString)
    {
        TestSetHappyPath<std::string>("Hello world", ProfilingProperty::kNumberOfPreparationFrames, DD_PROFILING_PROPERTY_TYPE_STRING);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetInt8)
    {
        TestSetHappyPath<int8_t>(-123, ProfilingProperty::kShaderEngineInstructionTraceMask, DD_PROFILING_PROPERTY_TYPE_INT8);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetInt16)
    {
        TestSetHappyPath<int16_t>(-2530, ProfilingProperty::kEnableSpm, DD_PROFILING_PROPERTY_TYPE_INT16);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetInt32)
    {
        TestSetHappyPath<int32_t>(-25123123, ProfilingProperty::kSpmSampleFrequency, DD_PROFILING_PROPERTY_TYPE_INT32);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetInt64)
    {
        TestSetHappyPath<int64_t>(-251231123112311, ProfilingProperty::kSpmMemoryLimit, DD_PROFILING_PROPERTY_TYPE_INT64);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetUInt8)
    {
        TestSetHappyPath<uint8_t>(123, ProfilingProperty::kSqttMemoryLimit, DD_PROFILING_PROPERTY_TYPE_UINT8);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetUInt16)
    {
        TestSetHappyPath<uint16_t>(2530, ProfilingProperty::kTriggerMode, DD_PROFILING_PROPERTY_TYPE_UINT16);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetUInt32)
    {
        TestSetHappyPath<uint32_t>(25123123, ProfilingProperty::kTriggerMarkerBegin, DD_PROFILING_PROPERTY_TYPE_UINT32);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSetUInt64)
    {
        TestSetHappyPath<uint64_t>(251231123112311, ProfilingProperty::kTriggerMarkerEnd, DD_PROFILING_PROPERTY_TYPE_UINT64);
    }

    // Get happy path tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RgpTraceSourceConfigTest, TestGetBool)
    {
        TestGetHappyPath<bool>(true, ProfilingProperty::kEnableInstructionTracing, DD_PROFILING_PROPERTY_TYPE_BOOL);
    }

    TEST_F(RgpTraceSourceConfigTest, TestGetFloat)
    {
        TestGetHappyPath<float>(124.0123f, ProfilingProperty::kInstructionTracingApiPsoHash, DD_PROFILING_PROPERTY_TYPE_FLOAT);
    }

    TEST_F(RgpTraceSourceConfigTest, TestGetString)
    {
        TestGetHappyPath<std::string>("Hello world", ProfilingProperty::kNumberOfPreparationFrames, DD_PROFILING_PROPERTY_TYPE_STRING);
    }

    TEST_F(RgpTraceSourceConfigTest, TestGetInt8)
    {
        TestGetHappyPath<int8_t>(-123, ProfilingProperty::kShaderEngineInstructionTraceMask, DD_PROFILING_PROPERTY_TYPE_INT8);
    }

    TEST_F(RgpTraceSourceConfigTest, TestGetInt16)
    {
        TestGetHappyPath<int16_t>(-2530, ProfilingProperty::kEnableSpm, DD_PROFILING_PROPERTY_TYPE_INT16);
    }

    TEST_F(RgpTraceSourceConfigTest, TestGetInt32)
    {
        TestGetHappyPath<int32_t>(-25123123, ProfilingProperty::kSpmSampleFrequency, DD_PROFILING_PROPERTY_TYPE_INT32);
    }

    TEST_F(RgpTraceSourceConfigTest, TestGetInt64)
    {
        TestGetHappyPath<int64_t>(-251231123112311, ProfilingProperty::kSpmMemoryLimit, DD_PROFILING_PROPERTY_TYPE_INT64);
    }

    TEST_F(RgpTraceSourceConfigTest, TestGetUInt8)
    {
        TestGetHappyPath<uint8_t>(123, ProfilingProperty::kSqttMemoryLimit, DD_PROFILING_PROPERTY_TYPE_UINT8);
    }

    TEST_F(RgpTraceSourceConfigTest, TestGetUInt16)
    {
        TestGetHappyPath<uint16_t>(2530, ProfilingProperty::kTriggerMode, DD_PROFILING_PROPERTY_TYPE_UINT16);
    }

    TEST_F(RgpTraceSourceConfigTest, TestGetUInt32)
    {
        TestGetHappyPath<uint32_t>(25123123, ProfilingProperty::kTriggerMarkerBegin, DD_PROFILING_PROPERTY_TYPE_UINT32);
    }

    TEST_F(RgpTraceSourceConfigTest, TestGetUInt64)
    {
        TestGetHappyPath<uint64_t>(251231123112311, ProfilingProperty::kTriggerMarkerEnd, DD_PROFILING_PROPERTY_TYPE_UINT64);
    }

    // Snapshot tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotWorks)
    {
        MockGetPropertyDescriptions();

        const float float_value = 124.0123f;
        MockGet<bool>(true, ProfilingProperty::kEnableInstructionTracing, DD_PROFILING_PROPERTY_TYPE_BOOL);
        MockGet<float>(float_value, ProfilingProperty::kInstructionTracingApiPsoHash, DD_PROFILING_PROPERTY_TYPE_FLOAT);
        MockGet<std::string>("Hello world", ProfilingProperty::kNumberOfPreparationFrames, DD_PROFILING_PROPERTY_TYPE_STRING);
        MockGet<int8_t>(-123, ProfilingProperty::kShaderEngineInstructionTraceMask, DD_PROFILING_PROPERTY_TYPE_INT8);
        MockGet<int16_t>(-2530, ProfilingProperty::kEnableSpm, DD_PROFILING_PROPERTY_TYPE_INT16);
        MockGet<int32_t>(-25123120, ProfilingProperty::kSpmSampleFrequency, DD_PROFILING_PROPERTY_TYPE_INT32);
        MockGet<int64_t>(-2512311231123113, ProfilingProperty::kSpmMemoryLimit, DD_PROFILING_PROPERTY_TYPE_INT64);
        MockGet<uint8_t>(123, ProfilingProperty::kSqttMemoryLimit, DD_PROFILING_PROPERTY_TYPE_UINT8);
        MockGet<uint16_t>(2530, ProfilingProperty::kTriggerMode, DD_PROFILING_PROPERTY_TYPE_UINT16);
        MockGet<uint32_t>(25123120, ProfilingProperty::kTriggerMarkerBegin, DD_PROFILING_PROPERTY_TYPE_UINT32);
        MockGet<uint64_t>(2512311231123113, ProfilingProperty::kTriggerMarkerEnd, DD_PROFILING_PROPERTY_TYPE_UINT64);

        RgpTraceSourceConfigSnapshot snapshot;

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());
        EXPECT_EQ(Result::kSuccess, config.GetSnapshot(snapshot));

        ASSERT_EQ(11, snapshot.size());

        // Check values
        EXPECT_EQ(1, snapshot.at(ProfilingProperty::kEnableInstructionTracing).data.bVal);
        EXPECT_EQ(float_value, snapshot.at(ProfilingProperty::kInstructionTracingApiPsoHash).data.fVal);
        EXPECT_EQ(0, strcmp(snapshot.at(ProfilingProperty::kNumberOfPreparationFrames).data.sVal, "Hello world"));

        EXPECT_EQ(-123, snapshot.at(ProfilingProperty::kShaderEngineInstructionTraceMask).data.i8Val);
        EXPECT_EQ(-2530, snapshot.at(ProfilingProperty::kEnableSpm).data.i16Val);
        EXPECT_EQ(-25123120, snapshot.at(ProfilingProperty::kSpmSampleFrequency).data.i32Val);
        EXPECT_EQ(-2512311231123113, snapshot.at(ProfilingProperty::kSpmMemoryLimit).data.i64Val);

        EXPECT_EQ(123, snapshot.at(ProfilingProperty::kSqttMemoryLimit).data.u8Val);
        EXPECT_EQ(2530, snapshot.at(ProfilingProperty::kTriggerMode).data.u16Val);
        EXPECT_EQ(25123120, snapshot.at(ProfilingProperty::kTriggerMarkerBegin).data.u32Val);
        EXPECT_EQ(2512311231123113, snapshot.at(ProfilingProperty::kTriggerMarkerEnd).data.u64Val);

        // Check types
        EXPECT_EQ(DD_PROFILING_PROPERTY_TYPE_BOOL, snapshot.at(ProfilingProperty::kEnableInstructionTracing).type);
        EXPECT_EQ(DD_PROFILING_PROPERTY_TYPE_FLOAT, snapshot.at(ProfilingProperty::kInstructionTracingApiPsoHash).type);
        EXPECT_EQ(DD_PROFILING_PROPERTY_TYPE_STRING, snapshot.at(ProfilingProperty::kNumberOfPreparationFrames).type);

        EXPECT_EQ(DD_PROFILING_PROPERTY_TYPE_INT8, snapshot.at(ProfilingProperty::kShaderEngineInstructionTraceMask).type);
        EXPECT_EQ(DD_PROFILING_PROPERTY_TYPE_INT16, snapshot.at(ProfilingProperty::kEnableSpm).type);
        EXPECT_EQ(DD_PROFILING_PROPERTY_TYPE_INT32, snapshot.at(ProfilingProperty::kSpmSampleFrequency).type);
        EXPECT_EQ(DD_PROFILING_PROPERTY_TYPE_INT64, snapshot.at(ProfilingProperty::kSpmMemoryLimit).type);

        EXPECT_EQ(DD_PROFILING_PROPERTY_TYPE_UINT8, snapshot.at(ProfilingProperty::kSqttMemoryLimit).type);
        EXPECT_EQ(DD_PROFILING_PROPERTY_TYPE_UINT16, snapshot.at(ProfilingProperty::kTriggerMode).type);
        EXPECT_EQ(DD_PROFILING_PROPERTY_TYPE_UINT32, snapshot.at(ProfilingProperty::kTriggerMarkerBegin).type);
        EXPECT_EQ(DD_PROFILING_PROPERTY_TYPE_UINT64, snapshot.at(ProfilingProperty::kTriggerMarkerEnd).type);
    }

    TEST_F(RgpTraceSourceConfigTest, TestEmptySnapshot)
    {
        EXPECT_CALL(rgp_api_mock, GetPropertyDescriptions(kMockDataContext, ::testing::Ne(nullptr), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce([&](DDModuleDataContext data_context, uint32_t* out_num, const DDProfilingPropertyDescription** out_desc) {
                DEV_TRACE_UNUSED(data_context);
                *out_num  = 0;
                *out_desc = nullptr;

                return DD_RESULT_SUCCESS;
            });

        RgpTraceSourceConfigSnapshot snapshot;

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());
        EXPECT_EQ(Result::kSuccess, config.GetSnapshot(snapshot));
        EXPECT_TRUE(snapshot.empty());
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotFailsIfCantGetProperty)
    {
        MockGetPropertyDescriptions();

        MockGet<bool>(true, ProfilingProperty::kEnableInstructionTracing, DD_PROFILING_PROPERTY_TYPE_BOOL);
        MockGet<float>(128.0, ProfilingProperty::kInstructionTracingApiPsoHash, DD_PROFILING_PROPERTY_TYPE_FLOAT);
        MockGet<std::string>("Hello world", ProfilingProperty::kNumberOfPreparationFrames, DD_PROFILING_PROPERTY_TYPE_STRING);
        MockGet<int8_t>(-123, ProfilingProperty::kShaderEngineInstructionTraceMask, DD_PROFILING_PROPERTY_TYPE_INT8);

        uint32_t property = static_cast<uint32_t>(ProfilingProperty::kEnableSpm);
        EXPECT_CALL(rgp_api_mock, GetPropertyValue(kMockDataContext, ::testing::Eq(property), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        RgpTraceSourceConfigSnapshot snapshot;

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());
        EXPECT_EQ(Result::kFailure, config.GetSnapshot(snapshot));
    }

    // Snapshot compare tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareFailsWithDifferentNumberOfProps)
    {
        RgpTraceSourceConfigSnapshot snapshot;
        snapshot.insert(std::make_pair(ProfilingProperty::kEnableInstructionTracing, DDProfilingPropertyValue{}));

        MockGetPropertyDescriptions();
        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool comapre_result = true;
        EXPECT_EQ(Result::kSuccess, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_FALSE(comapre_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareFailsWithMissingProp)
    {
        RgpTraceSourceConfigSnapshot snapshot;
        snapshot.insert(std::make_pair(ProfilingProperty::kEnableSpm, DDProfilingPropertyValue{}));

        MockGetPropertyDescriptions(0);
        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool comapre_result = true;
        EXPECT_EQ(Result::kSuccess, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_FALSE(comapre_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareFailsWithGetPropertyFailure)
    {
        RgpTraceSourceConfigSnapshot snapshot;
        snapshot.insert(std::make_pair(ProfilingProperty::kEnableInstructionTracing, DDProfilingPropertyValue{}));

        MockGetPropertyDescriptions(0);

        uint32_t property = static_cast<uint32_t>(ProfilingProperty::kEnableInstructionTracing);
        EXPECT_CALL(rgp_api_mock, GetPropertyValue(kMockDataContext, ::testing::Eq(property), ::testing::Ne(nullptr)))
            .Times(1)
            .WillOnce(::testing::Return(DD_RESULT_UNKNOWN));

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool comapre_result = true;
        EXPECT_EQ(Result::kFailure, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_FALSE(comapre_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareFailsWithMismatchedPropType)
    {
        RgpTraceSourceConfigSnapshot snapshot;
        DDProfilingPropertyValue     snapshot_val{};
        snapshot_val.type      = DD_PROFILING_PROPERTY_TYPE_INT16;
        snapshot_val.data.bVal = 1;
        snapshot.insert(std::make_pair(ProfilingProperty::kEnableInstructionTracing, snapshot_val));

        MockGetPropertyDescriptions(0);
        MockGet<bool>(true, ProfilingProperty::kEnableInstructionTracing, DD_PROFILING_PROPERTY_TYPE_BOOL);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool comapre_result = true;
        EXPECT_EQ(Result::kSuccess, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_FALSE(comapre_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareFailsBool)
    {
        RgpTraceSourceConfigSnapshot snapshot;
        DDProfilingPropertyValue     snapshot_val{};
        snapshot_val.type      = DD_PROFILING_PROPERTY_TYPE_BOOL;
        snapshot_val.data.bVal = 1;
        snapshot.insert(std::make_pair(ProfilingProperty::kEnableInstructionTracing, snapshot_val));

        MockGetPropertyDescriptions(0);
        MockGet<bool>(false, ProfilingProperty::kEnableInstructionTracing, DD_PROFILING_PROPERTY_TYPE_BOOL);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool comapre_result = true;
        EXPECT_EQ(Result::kSuccess, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_FALSE(comapre_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareFailsFloat)
    {
        RgpTraceSourceConfigSnapshot snapshot;
        DDProfilingPropertyValue     snapshot_val{};
        snapshot_val.type      = DD_PROFILING_PROPERTY_TYPE_FLOAT;
        snapshot_val.data.fVal = 0.1231f;
        snapshot.insert(std::make_pair(ProfilingProperty::kInstructionTracingApiPsoHash, snapshot_val));

        MockGetPropertyDescriptions(1);
        MockGet<float>(1234.0, ProfilingProperty::kInstructionTracingApiPsoHash, DD_PROFILING_PROPERTY_TYPE_FLOAT);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool comapre_result = true;
        EXPECT_EQ(Result::kSuccess, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_FALSE(comapre_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareFailsString)
    {
        RgpTraceSourceConfigSnapshot snapshot;
        DDProfilingPropertyValue     snapshot_val{};
        snapshot_val.type = DD_PROFILING_PROPERTY_TYPE_STRING;
        strcpy(snapshot_val.data.sVal, "hello");
        snapshot.insert(std::make_pair(ProfilingProperty::kNumberOfPreparationFrames, snapshot_val));

        MockGetPropertyDescriptions(2);
        MockGet<std::string>("bye", ProfilingProperty::kNumberOfPreparationFrames, DD_PROFILING_PROPERTY_TYPE_STRING);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool comapre_result = true;
        EXPECT_EQ(Result::kSuccess, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_FALSE(comapre_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareFailsInt8)
    {
        RgpTraceSourceConfigSnapshot snapshot;
        DDProfilingPropertyValue     snapshot_val{};
        snapshot_val.type       = DD_PROFILING_PROPERTY_TYPE_INT8;
        snapshot_val.data.i8Val = 21;
        snapshot.insert(std::make_pair(ProfilingProperty::kShaderEngineInstructionTraceMask, snapshot_val));

        MockGetPropertyDescriptions(3);
        MockGet<int8_t>(85, ProfilingProperty::kShaderEngineInstructionTraceMask, DD_PROFILING_PROPERTY_TYPE_INT8);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool comapre_result = true;
        EXPECT_EQ(Result::kSuccess, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_FALSE(comapre_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareFailsInt16)
    {
        RgpTraceSourceConfigSnapshot snapshot;
        DDProfilingPropertyValue     snapshot_val{};
        snapshot_val.type        = DD_PROFILING_PROPERTY_TYPE_INT16;
        snapshot_val.data.i16Val = 21;
        snapshot.insert(std::make_pair(ProfilingProperty::kEnableSpm, snapshot_val));

        MockGetPropertyDescriptions(4);
        MockGet<int16_t>(85, ProfilingProperty::kEnableSpm, DD_PROFILING_PROPERTY_TYPE_INT16);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool comapre_result = true;
        EXPECT_EQ(Result::kSuccess, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_FALSE(comapre_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareFailsInt32)
    {
        RgpTraceSourceConfigSnapshot snapshot;
        DDProfilingPropertyValue     snapshot_val{};
        snapshot_val.type        = DD_PROFILING_PROPERTY_TYPE_INT32;
        snapshot_val.data.i32Val = 21;
        snapshot.insert(std::make_pair(ProfilingProperty::kSpmSampleFrequency, snapshot_val));

        MockGetPropertyDescriptions(5);
        MockGet<int32_t>(85, ProfilingProperty::kSpmSampleFrequency, DD_PROFILING_PROPERTY_TYPE_INT32);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool comapre_result = true;
        EXPECT_EQ(Result::kSuccess, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_FALSE(comapre_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareFailsInt64)
    {
        RgpTraceSourceConfigSnapshot snapshot;
        DDProfilingPropertyValue     snapshot_val{};
        snapshot_val.type        = DD_PROFILING_PROPERTY_TYPE_INT64;
        snapshot_val.data.i64Val = 21;
        snapshot.insert(std::make_pair(ProfilingProperty::kSpmMemoryLimit, snapshot_val));

        MockGetPropertyDescriptions(6);
        MockGet<int64_t>(85, ProfilingProperty::kSpmMemoryLimit, DD_PROFILING_PROPERTY_TYPE_INT64);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool comapre_result = true;
        EXPECT_EQ(Result::kSuccess, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_FALSE(comapre_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareFailsUint8)
    {
        RgpTraceSourceConfigSnapshot snapshot;
        DDProfilingPropertyValue     snapshot_val{};
        snapshot_val.type       = DD_PROFILING_PROPERTY_TYPE_UINT8;
        snapshot_val.data.u8Val = 21;
        snapshot.insert(std::make_pair(ProfilingProperty::kSqttMemoryLimit, snapshot_val));

        MockGetPropertyDescriptions(7);
        MockGet<uint8_t>(85, ProfilingProperty::kSqttMemoryLimit, DD_PROFILING_PROPERTY_TYPE_UINT8);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool comapre_result = true;
        EXPECT_EQ(Result::kSuccess, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_FALSE(comapre_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareFailsUint16)
    {
        RgpTraceSourceConfigSnapshot snapshot;
        DDProfilingPropertyValue     snapshot_val{};
        snapshot_val.type        = DD_PROFILING_PROPERTY_TYPE_UINT16;
        snapshot_val.data.u16Val = 21;
        snapshot.insert(std::make_pair(ProfilingProperty::kTriggerMode, snapshot_val));

        MockGetPropertyDescriptions(8);
        MockGet<uint16_t>(85, ProfilingProperty::kTriggerMode, DD_PROFILING_PROPERTY_TYPE_UINT16);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool comapre_result = true;
        EXPECT_EQ(Result::kSuccess, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_FALSE(comapre_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareFailsUint32)
    {
        RgpTraceSourceConfigSnapshot snapshot;
        DDProfilingPropertyValue     snapshot_val{};
        snapshot_val.type        = DD_PROFILING_PROPERTY_TYPE_UINT32;
        snapshot_val.data.u32Val = 21;
        snapshot.insert(std::make_pair(ProfilingProperty::kTriggerMarkerBegin, snapshot_val));

        MockGetPropertyDescriptions(9);
        MockGet<uint32_t>(85, ProfilingProperty::kTriggerMarkerBegin, DD_PROFILING_PROPERTY_TYPE_UINT32);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool comapre_result = true;
        EXPECT_EQ(Result::kSuccess, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_FALSE(comapre_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareFailsUint64)
    {
        RgpTraceSourceConfigSnapshot snapshot;
        DDProfilingPropertyValue     snapshot_val{};
        snapshot_val.type        = DD_PROFILING_PROPERTY_TYPE_UINT64;
        snapshot_val.data.u64Val = 21;
        snapshot.insert(std::make_pair(ProfilingProperty::kTriggerMarkerEnd, snapshot_val));

        MockGetPropertyDescriptions(10);
        MockGet<uint64_t>(85, ProfilingProperty::kTriggerMarkerEnd, DD_PROFILING_PROPERTY_TYPE_UINT64);

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());

        bool comapre_result = true;
        EXPECT_EQ(Result::kSuccess, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_FALSE(comapre_result);
    }

    TEST_F(RgpTraceSourceConfigTest, TestSnapshotCompareSucceeds)
    {
        MockGetPropertyDescriptions();

        const float float_value = 124.0123f;
        MockGet<bool>(true, ProfilingProperty::kEnableInstructionTracing, DD_PROFILING_PROPERTY_TYPE_BOOL, 2);
        MockGet<float>(float_value, ProfilingProperty::kInstructionTracingApiPsoHash, DD_PROFILING_PROPERTY_TYPE_FLOAT, 2);
        MockGet<std::string>("Hello world", ProfilingProperty::kNumberOfPreparationFrames, DD_PROFILING_PROPERTY_TYPE_STRING, 2);
        MockGet<int8_t>(-123, ProfilingProperty::kShaderEngineInstructionTraceMask, DD_PROFILING_PROPERTY_TYPE_INT8, 2);
        MockGet<int16_t>(-2530, ProfilingProperty::kEnableSpm, DD_PROFILING_PROPERTY_TYPE_INT16, 2);
        MockGet<int32_t>(-251231233, ProfilingProperty::kSpmSampleFrequency, DD_PROFILING_PROPERTY_TYPE_INT32, 2);
        MockGet<int64_t>(-2512311231123112330, ProfilingProperty::kSpmMemoryLimit, DD_PROFILING_PROPERTY_TYPE_INT64, 2);
        MockGet<uint8_t>(123, ProfilingProperty::kSqttMemoryLimit, DD_PROFILING_PROPERTY_TYPE_UINT8, 2);
        MockGet<uint16_t>(2530, ProfilingProperty::kTriggerMode, DD_PROFILING_PROPERTY_TYPE_UINT16, 2);
        MockGet<uint32_t>(2512312330, ProfilingProperty::kTriggerMarkerBegin, DD_PROFILING_PROPERTY_TYPE_UINT32, 2);
        MockGet<uint64_t>(2512311231123112330, ProfilingProperty::kTriggerMarkerEnd, DD_PROFILING_PROPERTY_TYPE_UINT64, 2);

        RgpTraceSourceConfigSnapshot snapshot;

        RgpTraceSourceConfig config(kMockDataContext, &kMockRgpApi);
        EXPECT_EQ(Result::kSuccess, config.LoadAvailableProperties());
        EXPECT_EQ(Result::kSuccess, config.GetSnapshot(snapshot));

        ASSERT_EQ(11, snapshot.size());

        bool comapre_result = false;
        EXPECT_EQ(Result::kSuccess, config.CompareToSnapshot(snapshot, comapre_result));
        EXPECT_TRUE(comapre_result);
    }

}  // namespace devtrace
