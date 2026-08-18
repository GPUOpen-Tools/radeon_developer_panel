// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Tests for the userdata mappers.

#include <cstdint>
#include <string>

#include <json_utils.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <gtest/gtest.h>

#include "source_userdata.h"
#include "source_userdata_mapper.h"

namespace devtrace
{
    // Test suite / fixture
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class UserdataMapperTest : public ::testing::Test
    {
    protected:
        UserdataMapperTest()
        {
            JsonUtils::LoadJsonFile(RMV_USERDATA_JSON_FILENAME, rmv_json_string);
            rmv_userdata_json.Parse(rmv_json_string.c_str());
            JsonUtils::LoadJsonFile(RGP_USERDATA_JSON_FILENAME, rgp_json_string);
            rgp_userdata_json.Parse(rgp_json_string.c_str());
            JsonUtils::LoadJsonFile(RRA_USERDATA_JSON_FILENAME, rra_json_string);
            rra_userdata_json.Parse(rra_json_string.c_str());
            JsonUtils::LoadJsonFile(RGD_USERDATA_JSON_FILENAME, rgd_json_string);
            rgd_userdata_json.Parse(rgd_json_string.c_str());
        }

        static std::string rmv_json_string;
        static std::string rgp_json_string;
        static std::string rra_json_string;
        static std::string rgd_json_string;

        rapidjson::Document rmv_userdata_json;
        rapidjson::Document rgp_userdata_json;
        rapidjson::Document rra_userdata_json;
        rapidjson::Document rgd_userdata_json;

        RmvUserdata rmv_userdata;
        RgpUserdata rgp_userdata;
        RraUserdata rra_userdata;
        RgdUserdata rgd_userdata;

        bool MapRmv(const std::string& default_output_path)
        {
            RmvUserdataMapper                          mapper;
            rapidjson::StringBuffer                    buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            rmv_userdata_json.Accept(writer);
            std::string data(buffer.GetString());
            return mapper.Parse(reinterpret_cast<const void*>(data.c_str()), data.length() + 1, default_output_path, rmv_userdata).has_value();
        }

        bool MapRgp(const std::string& default_output_path)
        {
            RgpUserdataMapper                          mapper;
            rapidjson::StringBuffer                    buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            rgp_userdata_json.Accept(writer);
            std::string data(buffer.GetString());
            return mapper.Parse(reinterpret_cast<const void*>(data.c_str()), data.length() + 1, default_output_path, rgp_userdata).has_value();
        }

        bool MapRra(const std::string& default_output_path)
        {
            RraUserdataMapper                          mapper;
            rapidjson::StringBuffer                    buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            rra_userdata_json.Accept(writer);
            std::string data(buffer.GetString());
            return mapper.Parse(reinterpret_cast<const void*>(data.c_str()), data.length() + 1, default_output_path, rra_userdata).has_value();
        }

        bool MapRgd(const std::string& default_output_path)
        {
            RgdUserdataMapper                          mapper;
            rapidjson::StringBuffer                    buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            rgd_userdata_json.Accept(writer);
            std::string data(buffer.GetString());
            return mapper.Parse(reinterpret_cast<const void*>(data.c_str()), data.length() + 1, default_output_path, rgd_userdata).has_value();
        }
    };

    std::string UserdataMapperTest::rmv_json_string;
    std::string UserdataMapperTest::rgp_json_string;
    std::string UserdataMapperTest::rra_json_string;
    std::string UserdataMapperTest::rgd_json_string;

    // Happy path tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(UserdataMapperTest, TestOutputPathRmv)
    {
        EXPECT_TRUE(MapRmv(""));
        EXPECT_EQ(R"(C:\Users\developer\Documents\rmv_profiles\$(APP_NAME))", rmv_userdata.output_path);
    }

    TEST_F(UserdataMapperTest, TestOutputPathRgp)
    {
        EXPECT_TRUE(MapRgp(""));
        EXPECT_EQ(R"(C:\Users\developer\Documents\rgp_profiles\$(APP_NAME))", rgp_userdata.output_path);
    }

    TEST_F(UserdataMapperTest, TestOutputPathRra)
    {
        EXPECT_TRUE(MapRra(""));
        EXPECT_EQ(R"(C:\Users\developer\Documents\rra_scenes\$(APP_NAME))", rra_userdata.output_path);
    }

    TEST_F(UserdataMapperTest, TestOutputPathRgd)
    {
        EXPECT_TRUE(MapRgd(""));
        EXPECT_EQ(R"(C:\Users\developer\Documents\rgd_dumps\$(APP_NAME))", rgd_userdata.output_path);
    }

    TEST_F(UserdataMapperTest, TestRgpParsesCorrectly)
    {
        // Ensure that the enum is the right width
        EXPECT_EQ(sizeof(uint8_t), sizeof(ComputeAutoCaptureMode));

        EXPECT_TRUE(MapRgp(""));
        EXPECT_EQ(ComputeAutoCaptureMode::kTimer, rgp_userdata.opencl_auto_trigger);
        EXPECT_EQ(25, rgp_userdata.dispatch_count);
        EXPECT_EQ(123, rgp_userdata.compute_auto_capture_time_ms);
        EXPECT_EQ(true, rgp_userdata.use_frame_trigger);

        EXPECT_EQ("", rgp_userdata.spm_counters_path);
    }

    TEST_F(UserdataMapperTest, TestRgpParsesBoolOpencl)
    {
        rgp_userdata_json[kOpenClAutoTriggerKey].SetBool(true);

        EXPECT_TRUE(MapRgp(""));
        EXPECT_EQ(ComputeAutoCaptureMode::kDispatchIndices, rgp_userdata.opencl_auto_trigger);
    }

    TEST_F(UserdataMapperTest, TestRgdParsesCorrectly)
    {
        EXPECT_TRUE(MapRgd(""));
        EXPECT_TRUE(rgd_userdata.generate_text_summary);
        EXPECT_TRUE(rgd_userdata.generate_json_summary);
        EXPECT_TRUE(rgd_userdata.summary_options.show_marker_source);
        EXPECT_TRUE(rgd_userdata.summary_options.expand_markers);
    }

    // Fallback output path tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(UserdataMapperTest, TestFallbackOutputPathRmv)
    {
        rmv_userdata_json.RemoveMember("output_path");

        EXPECT_TRUE(MapRmv("Fallback rmv path"));
        EXPECT_EQ("Fallback rmv path", rmv_userdata.output_path);
    }

    TEST_F(UserdataMapperTest, TestFallbackOutputPathRgp)
    {
        rgp_userdata_json.RemoveMember("output_path");

        EXPECT_TRUE(MapRgp("Fallback rgp path"));
        EXPECT_EQ("Fallback rgp path", rgp_userdata.output_path);
    }

    TEST_F(UserdataMapperTest, TestFallbackOutputPathRra)
    {
        rra_userdata_json.RemoveMember("output_path");

        EXPECT_TRUE(MapRra("Fallback rra path"));
        EXPECT_EQ("Fallback rra path\0", rra_userdata.output_path);
    }

    TEST_F(UserdataMapperTest, TestFallbackOutputPathRgd)
    {
        rgd_userdata_json.RemoveMember("output_path");

        EXPECT_TRUE(MapRgd("Fallback rgd path"));
        EXPECT_EQ("Fallback rgd path", rgd_userdata.output_path);
    }

    // Bad JSON tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(UserdataMapperTest, TestRmvNullBuffer)
    {
        RmvUserdataMapper mapper;
        EXPECT_FALSE(mapper.Parse(nullptr, 0, "", rmv_userdata).has_value());
    }

    TEST_F(UserdataMapperTest, TestRmvEmptyBuffer)
    {
        RmvUserdataMapper mapper;
        EXPECT_FALSE(mapper.Parse("", 0, "", rmv_userdata).has_value());
    }

    TEST_F(UserdataMapperTest, TestRmvEmptyString)
    {
        RmvUserdataMapper mapper;
        EXPECT_FALSE(mapper.Parse("", 1, "", rmv_userdata).has_value());
    }

    TEST_F(UserdataMapperTest, TestRgpNullBuffer)
    {
        RgpUserdataMapper mapper;
        EXPECT_FALSE(mapper.Parse(nullptr, 0, "", rgp_userdata).has_value());
    }

    TEST_F(UserdataMapperTest, TestRgpEmptyBuffer)
    {
        RgpUserdataMapper mapper;
        EXPECT_FALSE(mapper.Parse("", 0, "", rgp_userdata).has_value());
    }

    TEST_F(UserdataMapperTest, TestRgpEmptyString)
    {
        RgpUserdataMapper mapper;
        EXPECT_FALSE(mapper.Parse("", 1, "", rgp_userdata).has_value());
    }

    TEST_F(UserdataMapperTest, TestRraNullBuffer)
    {
        RraUserdataMapper mapper;
        EXPECT_FALSE(mapper.Parse(nullptr, 0, "", rra_userdata).has_value());
    }

    TEST_F(UserdataMapperTest, TestRraEmptyBuffer)
    {
        RraUserdataMapper mapper;
        EXPECT_FALSE(mapper.Parse("", 0, "", rra_userdata).has_value());
    }

    TEST_F(UserdataMapperTest, TestRraEmptyString)
    {
        RraUserdataMapper mapper;
        EXPECT_FALSE(mapper.Parse("", 1, "", rra_userdata).has_value());
    }

    TEST_F(UserdataMapperTest, TestRgdNullBuffer)
    {
        RgdUserdataMapper mapper;
        EXPECT_FALSE(mapper.Parse(nullptr, 0, "", rgd_userdata).has_value());
    }

    TEST_F(UserdataMapperTest, TestRgdEmptyBuffer)
    {
        RgdUserdataMapper mapper;
        EXPECT_FALSE(mapper.Parse("", 0, "", rgd_userdata).has_value());
    }

    TEST_F(UserdataMapperTest, TestRgdEmptyString)
    {
        RgdUserdataMapper mapper;
        EXPECT_FALSE(mapper.Parse("", 1, "", rgd_userdata).has_value());
    }

    // Bad JSON tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(UserdataMapperTest, TestBadOutputPathRmv)
    {
        rmv_userdata_json["output_path"].SetBool(false);
        EXPECT_TRUE(MapRmv("bad"));
        EXPECT_EQ("bad", rmv_userdata.output_path);
    }

    TEST_F(UserdataMapperTest, TestBadOutputPathRgp)
    {
        rgp_userdata_json["output_path"].SetBool(false);
        EXPECT_TRUE(MapRgp("bad"));
        EXPECT_EQ("bad", rgp_userdata.output_path);
    }

    TEST_F(UserdataMapperTest, TestBadOutputPathRra)
    {
        rra_userdata_json["output_path"].SetBool(false);
        EXPECT_TRUE(MapRra("bad"));
        EXPECT_EQ("bad", rra_userdata.output_path);
    }

    TEST_F(UserdataMapperTest, TestBadOutputPathRgd)
    {
        rgd_userdata_json["output_path"].SetBool(false);
        EXPECT_TRUE(MapRgd("bad"));
        EXPECT_EQ("bad", rgd_userdata.output_path);
    }

    // Bad JSON RGP tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(UserdataMapperTest, TestBadOpenClAutoTrigger)
    {
        rgp_userdata_json["opencl_auto_trigger"].SetString("asd", rgp_userdata_json.GetAllocator());
        EXPECT_TRUE(MapRgp(""));
        EXPECT_EQ(ComputeAutoCaptureMode::kNone, rgp_userdata.opencl_auto_trigger);
    }

    TEST_F(UserdataMapperTest, TestBadDispatchCount)
    {
        rgp_userdata_json["dispatch_count"].SetString("bad", rgp_userdata_json.GetAllocator());
        EXPECT_TRUE(MapRgp(""));
        EXPECT_EQ(0, rgp_userdata.dispatch_count);
    }

    TEST_F(UserdataMapperTest, TestBadAutoCaptureTime)
    {
        rgp_userdata_json["compute_auto_capture_time_ms"].SetString("bad", rgp_userdata_json.GetAllocator());
        EXPECT_TRUE(MapRgp(""));
        EXPECT_EQ(0, rgp_userdata.compute_auto_capture_time_ms);
    }

    TEST_F(UserdataMapperTest, TestBadFrameTrigger)
    {
        rgp_userdata_json["frame_trigger"].SetInt(21);
        EXPECT_TRUE(MapRgp(""));
        EXPECT_EQ(0, rgp_userdata.use_frame_trigger);
    }

    // Bad JSON RGD tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(UserdataMapperTest, TestBadGenerateTextSummary)
    {
        rgd_userdata_json["generate_text_summary"].SetString("asd", rgd_userdata_json.GetAllocator());

        EXPECT_TRUE(MapRgd(""));
        EXPECT_FALSE(rgd_userdata.generate_text_summary);
    }

    TEST_F(UserdataMapperTest, TestBadGenerateJsonSummary)
    {
        rgd_userdata_json["generate_json_summary"].SetString("asd", rgd_userdata_json.GetAllocator());

        EXPECT_TRUE(MapRgd(""));
        EXPECT_FALSE(rgd_userdata.generate_json_summary);
    }

    TEST_F(UserdataMapperTest, TestBadShowMarkerSource)
    {
        rgd_userdata_json["show_marker_source"].SetString("asd", rgd_userdata_json.GetAllocator());

        EXPECT_TRUE(MapRgd(""));
        EXPECT_FALSE(rgd_userdata.summary_options.show_marker_source);
    }

    TEST_F(UserdataMapperTest, TestBadExpandMarkers)
    {
        rgd_userdata_json["expand_markers"].SetString("asd", rgd_userdata_json.GetAllocator());

        EXPECT_TRUE(MapRgd(""));
        EXPECT_FALSE(rgd_userdata.summary_options.expand_markers);
    }

    // Missing data tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(UserdataMapperTest, TestRgpMissingData)
    {
        rgp_userdata_json.RemoveMember("opencl_auto_trigger");
        rgp_userdata_json.RemoveMember("dispatch_count");
        rgp_userdata_json.RemoveMember("compute_auto_capture_time_ms");
        rgp_userdata_json.RemoveMember("frame_trigger");
        rgp_userdata_json.RemoveMember("spm_counter_path");

        EXPECT_TRUE(MapRgp(""));

        EXPECT_EQ(ComputeAutoCaptureMode::kNone, rgp_userdata.opencl_auto_trigger);
        EXPECT_EQ(0, rgp_userdata.dispatch_count);
        EXPECT_EQ(0, rgp_userdata.compute_auto_capture_time_ms);
        EXPECT_EQ(false, rgp_userdata.use_frame_trigger);
        EXPECT_EQ("", rgp_userdata.spm_counters_path);
    }

    TEST_F(UserdataMapperTest, TestRgdMissingData)
    {
        rgd_userdata_json.RemoveMember("generate_text_summary");
        rgd_userdata_json.RemoveMember("generate_json_summary");
        rgd_userdata_json.RemoveMember("show_marker_source");
        rgd_userdata_json.RemoveMember("expand_markers");

        EXPECT_TRUE(MapRgd(""));
        EXPECT_FALSE(rgd_userdata.generate_text_summary);
        EXPECT_FALSE(rgd_userdata.generate_json_summary);
        EXPECT_FALSE(rgd_userdata.summary_options.show_marker_source);
        EXPECT_FALSE(rgd_userdata.summary_options.expand_markers);
    }

    // Write tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(UserdataMapperTest, TestRmvWritesCorrectly)
    {
        rmv_userdata.output_path = "rmv_test_output_path";

        RmvUserdataMapper mapper;
        auto              result = mapper.Serialize(rmv_userdata);
        EXPECT_TRUE(result.has_value());

        auto        deserializer = devtrace::JsonMapper::Deserialize(result.value());
        const auto& json         = deserializer.Json();
        EXPECT_EQ(1u, json.MemberCount());
        EXPECT_EQ(rmv_userdata.output_path, std::string(json["output_path"].GetString()));
    }

    TEST_F(UserdataMapperTest, TestRraWritesCorrectly)
    {
        rra_userdata.output_path = "rra_test_output_path";

        RraUserdataMapper mapper;
        auto              result = mapper.Serialize(rra_userdata);
        EXPECT_TRUE(result.has_value());

        auto        deserializer = devtrace::JsonMapper::Deserialize(result.value());
        const auto& json         = deserializer.Json();
        EXPECT_EQ(3u, json.MemberCount());
        EXPECT_EQ(rra_userdata.output_path, std::string(json["output_path"].GetString()));
    }

    TEST_F(UserdataMapperTest, TestRgdWritesCorrectly)
    {
        rgd_userdata.output_path                        = "rgd_test_output_path";
        rgd_userdata.generate_text_summary              = true;
        rgd_userdata.generate_json_summary              = true;
        rgd_userdata.summary_options.show_marker_source = true;
        rgd_userdata.summary_options.expand_markers     = true;

        RgdUserdataMapper mapper;
        auto              result = mapper.Serialize(rgd_userdata);
        EXPECT_TRUE(result.has_value());

        auto        deserializer = devtrace::JsonMapper::Deserialize(result.value());
        const auto& json         = deserializer.Json();
        EXPECT_EQ(5u, json.MemberCount());
        EXPECT_EQ(rgd_userdata.output_path, std::string(json["output_path"].GetString()));

        EXPECT_EQ(true, json["generate_text_summary"].GetBool());
        EXPECT_EQ(true, json["generate_json_summary"].GetBool());
        EXPECT_EQ(true, json["show_marker_source"].GetBool());
        EXPECT_EQ(true, json["expand_markers"].GetBool());
    }

    TEST_F(UserdataMapperTest, TestRgpWritesCorrectly)
    {
        rgp_userdata.output_path                  = "rgp_test_output_path";
        rgp_userdata.use_frame_trigger            = true;
        rgp_userdata.opencl_auto_trigger          = ComputeAutoCaptureMode::kTimer;
        rgp_userdata.dispatch_count               = 89;
        rgp_userdata.compute_auto_capture_time_ms = 1912;
        rgp_userdata.spm_counters_path            = "rgp_spm_counters_path";

        RgpUserdataMapper mapper;
        auto              result = mapper.Serialize(rgp_userdata);
        EXPECT_TRUE(result.has_value());

        auto        deserializer = devtrace::JsonMapper::Deserialize(result.value());
        const auto& json         = deserializer.Json();

        EXPECT_EQ(5u, json.MemberCount());

        EXPECT_EQ(rgp_userdata.output_path, std::string(json["output_path"].GetString()));
        EXPECT_EQ(rgp_userdata.use_frame_trigger, json["frame_trigger"].GetBool());
        EXPECT_EQ(static_cast<int>(rgp_userdata.opencl_auto_trigger), json["opencl_auto_trigger"].GetInt());
        EXPECT_EQ(rgp_userdata.dispatch_count, json["dispatch_count"].GetUint());
        EXPECT_EQ(rgp_userdata.compute_auto_capture_time_ms, json["compute_auto_capture_time_ms"].GetUint());
    }

};  // namespace devtrace
