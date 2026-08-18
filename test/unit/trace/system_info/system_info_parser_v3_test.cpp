// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Tests for the system info parser V3.

#include <algorithm>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include <json_utils.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "system_info_parser.h"

namespace devtrace
{
    class SystemInfoParserV3Test : public ::testing::Test
    {
    protected:
        SystemInfoParserV3Test()
        {
            JsonUtils::LoadJsonFile(SYSTEM_INFO_V3_JSON_FILENAME, json_string);
            json.Parse(json_string.c_str());
        }

        bool Parse()
        {
            rapidjson::StringBuffer                    buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            json.Accept(writer);
            return parser.Parse(buffer.GetString(), parsed_info);
        }

        rapidjson::Document json;
        SystemInfoParser    parser;
        CompleteSystemInfo  parsed_info{};

    private:
        static std::string json_string;
    };

    std::string SystemInfoParserV3Test::json_string = "";

    // Test suite / fixture
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(SystemInfoParserV3Test, TestFailOnEmptyString)
    {
        EXPECT_FALSE(parser.Parse("", parsed_info));
    }

    TEST_F(SystemInfoParserV3Test, TestOkOnEmptyObject)
    {
        EXPECT_TRUE(parser.Parse("{}", parsed_info));
    }

    TEST_F(SystemInfoParserV3Test, TestHappyPath)
    {
        EXPECT_TRUE(Parse());

        EXPECT_EQ(1, parsed_info.stats_info.version);
        EXPECT_EQ(3346519563805, parsed_info.stats_info.timestamp.ticks);
        EXPECT_EQ(10000000, parsed_info.stats_info.timestamp.ticks_per_second);

        EXPECT_EQ(3, parsed_info.system_info.version);
        EXPECT_EQ(true, parsed_info.system_info.config_info.power_dpm_writable);
        EXPECT_EQ(10000000, parsed_info.system_info.cpu_info.timestamp_frequency);
        EXPECT_EQ(42, parsed_info.system_info.dev_driver_info.client_interface_major_version);
        EXPECT_EQ("dev", parsed_info.system_info.dev_driver_info.tag);
        EXPECT_EQ("AMD Windows", parsed_info.system_info.driver_info.name);
        EXPECT_EQ("AMD Windows Driver", parsed_info.system_info.driver_info.description);
        EXPECT_EQ("22.40-220717n-230356E-ATI", parsed_info.system_info.driver_info.packaging_version);
        EXPECT_EQ("22.40", parsed_info.system_info.driver_info.software_version);
        EXPECT_EQ(true, parsed_info.system_info.driver_info.is_closed_source);
        EXPECT_EQ(22, parsed_info.system_info.driver_info.packaging_version_major);
        EXPECT_EQ(40, parsed_info.system_info.driver_info.packaging_version_minor);
        EXPECT_EQ(true, parsed_info.system_info.etw_support_info.has_permission);
        EXPECT_EQ(true, parsed_info.system_info.etw_support_info.is_supported);
        EXPECT_EQ(0, parsed_info.system_info.etw_support_info.status_code);
        EXPECT_EQ("The operation completed successfully.\r\n", parsed_info.system_info.etw_support_info.status_description);

        ASSERT_EQ(1, parsed_info.system_info.gpus.size());
        auto& gpu = parsed_info.system_info.gpus[0];

        EXPECT_EQ(2044000000, gpu.asic.engine_clock_hz.max);
        EXPECT_EQ(500000000, gpu.asic.engine_clock_hz.min);
        EXPECT_EQ(100000000, gpu.asic.gpu_counter_freq);
        EXPECT_EQ(0, gpu.asic.gpu_index);
        EXPECT_EQ(29695, gpu.asic.id_info.device);
        EXPECT_EQ(60, gpu.asic.id_info.e_rev);
        EXPECT_EQ(143, gpu.asic.id_info.family);
        EXPECT_EQ(13, gpu.asic.id_info.gfx_engine);
        EXPECT_EQ(199, gpu.asic.id_info.revision);
        EXPECT_EQ(2021, gpu.big_sw.major);
        EXPECT_EQ(1, gpu.big_sw.minor);
        EXPECT_EQ(0, gpu.big_sw.misc);
        EXPECT_EQ(224000000000, gpu.memory.bandwidth);
        EXPECT_EQ(128, gpu.memory.bus_bit_width);
        EXPECT_EQ(875000000, gpu.memory.mem_clock_hz.max);
        EXPECT_EQ(96000000, gpu.memory.mem_clock_hz.min);
        EXPECT_EQ(16, gpu.memory.mem_ops_per_clock);
        EXPECT_EQ("Gddr6", gpu.memory.type);
        EXPECT_EQ("AMD Radeon RX 6600", gpu.name);
        EXPECT_EQ(11, gpu.pci.bus);
        EXPECT_EQ(0, gpu.pci.device);
        EXPECT_EQ(0, gpu.pci.function);

        ASSERT_EQ(1, gpu.memory.excluded_va_ranges.size());
        auto& excluded_range = gpu.memory.excluded_va_ranges[0];

        EXPECT_EQ(0, excluded_range.base);
        EXPECT_EQ(4096, excluded_range.size);

        ASSERT_EQ(2, gpu.memory.heaps.size());
        auto& invisible_heap = gpu.memory.heaps[0];
        EXPECT_EQ("invisible", invisible_heap.heap_type);
        EXPECT_EQ(268435456, invisible_heap.phys_addr);
        EXPECT_EQ(8304721920, invisible_heap.size);

        auto& local_heap = gpu.memory.heaps[1];
        EXPECT_EQ("local", local_heap.heap_type);
        EXPECT_EQ(0, local_heap.phys_addr);
        EXPECT_EQ(268435456, local_heap.size);

        EXPECT_EQ("19041.1.amd64fre.vb_release.191206-1406", parsed_info.system_info.os_info.desc);
        EXPECT_EQ("MSDN-DEV-01.amd.com", parsed_info.system_info.os_info.hostname);
        EXPECT_EQ(34277056512, parsed_info.system_info.os_info.memory.physical);
        EXPECT_EQ(68636794880, parsed_info.system_info.os_info.memory.swap);
        EXPECT_EQ("Windows 10 Pro", parsed_info.system_info.os_info.name);
    }

    // Stats tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(SystemInfoParserV3Test, TestUnknownStatsVersion)
    {
        json["stats"]["version"].SetUint(static_cast<uint32_t>(-1));
        EXPECT_FALSE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestNoStats)
    {
        json.RemoveMember("stats");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestNoStatsValue)
    {
        json["stats"].RemoveMember("value");
        EXPECT_FALSE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestNoStatsVersion)
    {
        json["stats"].RemoveMember("version");
        EXPECT_TRUE(Parse());

        EXPECT_EQ(1, parsed_info.stats_info.version);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingTimestamp)
    {
        json["stats"]["value"].RemoveMember("timestamp");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingTimestampData)
    {
        json["stats"]["value"]["timestamp"].RemoveMember("ticks");
        json["stats"]["value"]["timestamp"].RemoveMember("ticksPerSecond");
        EXPECT_TRUE(Parse());

        EXPECT_EQ(0, parsed_info.stats_info.timestamp.ticks);
        EXPECT_EQ(0, parsed_info.stats_info.timestamp.ticks_per_second);
    }

    // System tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(SystemInfoParserV3Test, TestUnknownSystemVersion)
    {
        json["system"]["version"].SetUint(static_cast<uint32_t>(-1));
        EXPECT_FALSE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestNoSystemInfo)
    {
        json.RemoveMember("system");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestNoSystemInfoValue)
    {
        json["system"].RemoveMember("value");
        EXPECT_FALSE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestNoSystemInfoVersion)
    {
        json["system"].RemoveMember("version");
        EXPECT_TRUE(Parse());

        EXPECT_EQ(1, parsed_info.system_info.version);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingSystemConfig)
    {
        json["system"]["value"].RemoveMember("config");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestNoPowerDpmWritable)
    {
        json["system"]["value"]["config"].RemoveMember("power_dpm_writable");
        EXPECT_TRUE(Parse());

        EXPECT_EQ(false, parsed_info.system_info.config_info.power_dpm_writable);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingCpu)
    {
        json["system"]["value"].RemoveMember("cpu");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingTimestampFreq)
    {
        json["system"]["value"]["cpu"].RemoveMember("timestampFrequency");
        EXPECT_TRUE(Parse());

        EXPECT_EQ(0, parsed_info.system_info.cpu_info.timestamp_frequency);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingDevdriver)
    {
        json["system"]["value"].RemoveMember("devdriver");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingDevdriverData)
    {
        json["system"]["value"]["devdriver"].RemoveMember("GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION");
        json["system"]["value"]["devdriver"].RemoveMember("tag");
        EXPECT_TRUE(Parse());

        EXPECT_EQ(0, parsed_info.system_info.dev_driver_info.client_interface_major_version);
        EXPECT_EQ("", parsed_info.system_info.dev_driver_info.tag);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingEtw)
    {
        json["system"]["value"].RemoveMember("etwSupport");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingEtwData)
    {
        json["system"]["value"]["etwSupport"].RemoveMember("hasPermission");
        json["system"]["value"]["etwSupport"].RemoveMember("isSupported");
        json["system"]["value"]["etwSupport"].RemoveMember("statusCode");
        json["system"]["value"]["etwSupport"].RemoveMember("statusDescription");

        EXPECT_TRUE(Parse());

        EXPECT_EQ(false, parsed_info.system_info.etw_support_info.has_permission);
        EXPECT_EQ(false, parsed_info.system_info.etw_support_info.is_supported);
        EXPECT_EQ(0, parsed_info.system_info.etw_support_info.status_code);
        EXPECT_EQ("", parsed_info.system_info.etw_support_info.status_description);
    }

    // Driver tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(SystemInfoParserV3Test, TestMissingDriver)
    {
        json["system"]["value"].RemoveMember("driver");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingDriverData)
    {
        json["system"]["value"]["driver"].RemoveMember("packagingVersion");
        json["system"]["value"]["driver"].RemoveMember("softwareVersion");
        json["system"]["value"]["driver"].RemoveMember("name");
        json["system"]["value"]["driver"].RemoveMember("description");
        EXPECT_TRUE(Parse());

        EXPECT_EQ("", parsed_info.system_info.driver_info.packaging_version);
        EXPECT_EQ("", parsed_info.system_info.driver_info.software_version);
        EXPECT_EQ("", parsed_info.system_info.driver_info.name);
        EXPECT_EQ("", parsed_info.system_info.driver_info.description);
        EXPECT_EQ(0, parsed_info.system_info.driver_info.packaging_version_major);
        EXPECT_EQ(0, parsed_info.system_info.driver_info.packaging_version_minor);
    }

    TEST_F(SystemInfoParserV3Test, TestEmptyPackageVersion)
    {
        json["system"]["value"]["driver"]["packagingVersion"].SetString("", json.GetAllocator());
        EXPECT_TRUE(Parse());

        EXPECT_EQ(0, parsed_info.system_info.driver_info.packaging_version_major);
        EXPECT_EQ(0, parsed_info.system_info.driver_info.packaging_version_minor);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingDot)
    {
        json["system"]["value"]["driver"]["packagingVersion"].SetString("22", json.GetAllocator());
        EXPECT_TRUE(Parse());

        EXPECT_EQ(0, parsed_info.system_info.driver_info.packaging_version_major);
        EXPECT_EQ(0, parsed_info.system_info.driver_info.packaging_version_minor);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingMinorVersionWithDot)
    {
        json["system"]["value"]["driver"]["packagingVersion"].SetString("22.", json.GetAllocator());
        EXPECT_TRUE(Parse());

        EXPECT_EQ(22, parsed_info.system_info.driver_info.packaging_version_major);
        EXPECT_EQ(0, parsed_info.system_info.driver_info.packaging_version_minor);
    }

    TEST_F(SystemInfoParserV3Test, TestMinorVersionMissingSecondDot)
    {
        json["system"]["value"]["driver"]["packagingVersion"].SetString("22.40", json.GetAllocator());
        EXPECT_TRUE(Parse());

        EXPECT_EQ(22, parsed_info.system_info.driver_info.packaging_version_major);
        EXPECT_EQ(0, parsed_info.system_info.driver_info.packaging_version_minor);
    }

    TEST_F(SystemInfoParserV3Test, TestInvalidDriverMajorVersion)
    {
        json["system"]["value"]["driver"]["packagingVersion"].SetString("hello.40-220717n-230356E-ATI", json.GetAllocator());
        EXPECT_TRUE(Parse());

        EXPECT_EQ(0, parsed_info.system_info.driver_info.packaging_version_major);
        EXPECT_EQ(40, parsed_info.system_info.driver_info.packaging_version_minor);
    }

    TEST_F(SystemInfoParserV3Test, TestInvalidDriverMinorVersion)
    {
        json["system"]["value"]["driver"]["packagingVersion"].SetString("22.hello-220717n-230356E-ATI", json.GetAllocator());
        EXPECT_TRUE(Parse());

        EXPECT_EQ(22, parsed_info.system_info.driver_info.packaging_version_major);
        EXPECT_EQ(0, parsed_info.system_info.driver_info.packaging_version_minor);
    }

    TEST_F(SystemInfoParserV3Test, TestInvalidDriverMajorMinorVersion)
    {
        json["system"]["value"]["driver"]["packagingVersion"].SetString("goodbye.hello-220717n-230356E-ATI", json.GetAllocator());
        EXPECT_TRUE(Parse());

        EXPECT_EQ(0, parsed_info.system_info.driver_info.packaging_version_major);
        EXPECT_EQ(0, parsed_info.system_info.driver_info.packaging_version_minor);
    }

    TEST_F(SystemInfoParserV3Test, TestWorksLinuxFlavor)
    {
        json["system"]["value"]["driver"]["packagingVersion"].SetString("22.40.50300.1446019~22.04", json.GetAllocator());
        EXPECT_TRUE(Parse());

        EXPECT_EQ(22, parsed_info.system_info.driver_info.packaging_version_major);
        EXPECT_EQ(40, parsed_info.system_info.driver_info.packaging_version_minor);
    }

    TEST_F(SystemInfoParserV3Test, TestNonProDriver)
    {
        json["system"]["value"]["driver"]["isClosedSource"].SetBool(false);
        EXPECT_TRUE(Parse());

        EXPECT_FALSE(parsed_info.system_info.driver_info.is_closed_source);
    }

    // GPU Tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(SystemInfoParserV3Test, TestNoGpus)
    {
        {
            auto& _arr = json["system"]["value"]["gpus"];
            _arr.Erase(_arr.Begin());
        }
        EXPECT_TRUE(Parse());

        EXPECT_TRUE(parsed_info.system_info.gpus.empty());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingAsic)
    {
        json["system"]["value"]["gpus"][0].RemoveMember("asic");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingEngineClock)
    {
        json["system"]["value"]["gpus"][0]["asic"].RemoveMember("engineClockHz");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingEngineClockData)
    {
        json["system"]["value"]["gpus"][0]["asic"]["engineClockHz"].RemoveMember("max");
        json["system"]["value"]["gpus"][0]["asic"]["engineClockHz"].RemoveMember("min");
        EXPECT_TRUE(Parse());

        ASSERT_FALSE(parsed_info.system_info.gpus.empty());
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].asic.engine_clock_hz.max);
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].asic.engine_clock_hz.min);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingAsicIds)
    {
        json["system"]["value"]["gpus"][0]["asic"].RemoveMember("ids");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingAsicIdData)
    {
        json["system"]["value"]["gpus"][0]["asic"]["ids"].RemoveMember("device");
        json["system"]["value"]["gpus"][0]["asic"]["ids"].RemoveMember("eRev");
        json["system"]["value"]["gpus"][0]["asic"]["ids"].RemoveMember("family");
        json["system"]["value"]["gpus"][0]["asic"]["ids"].RemoveMember("gfxEngine");
        json["system"]["value"]["gpus"][0]["asic"]["ids"].RemoveMember("revision");

        EXPECT_TRUE(Parse());

        ASSERT_FALSE(parsed_info.system_info.gpus.empty());
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].asic.id_info.device);
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].asic.id_info.e_rev);
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].asic.id_info.family);
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].asic.id_info.gfx_engine);
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].asic.id_info.revision);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingAsicData)
    {
        json["system"]["value"]["gpus"][0]["asic"].RemoveMember("gpuCounterFreq");
        json["system"]["value"]["gpus"][0]["asic"].RemoveMember("gpuIndex");
        EXPECT_TRUE(Parse());

        ASSERT_FALSE(parsed_info.system_info.gpus.empty());
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].asic.gpu_counter_freq);
        EXPECT_EQ(static_cast<uint32_t>(-1), parsed_info.system_info.gpus[0].asic.gpu_index);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingBigSw)
    {
        json["system"]["value"]["gpus"][0].RemoveMember("bigSw");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingBigSwData)
    {
        json["system"]["value"]["gpus"][0]["bigSw"].RemoveMember("major");
        json["system"]["value"]["gpus"][0]["bigSw"].RemoveMember("minor");
        json["system"]["value"]["gpus"][0]["bigSw"].RemoveMember("misc");
        EXPECT_TRUE(Parse());

        ASSERT_FALSE(parsed_info.system_info.gpus.empty());
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].big_sw.major);
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].big_sw.minor);
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].big_sw.misc);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingMemory)
    {
        json["system"]["value"]["gpus"][0].RemoveMember("memory");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingVaRanges)
    {
        json["system"]["value"]["gpus"][0]["memory"].RemoveMember("excludedVaRanges");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestEmptyVaRanges)
    {
        {
            auto& _arr = json["system"]["value"]["gpus"][0]["memory"]["excludedVaRanges"];
            _arr.Erase(_arr.Begin());
        }
        EXPECT_TRUE(Parse());

        ASSERT_FALSE(parsed_info.system_info.gpus.empty());
        EXPECT_TRUE(parsed_info.system_info.gpus[0].memory.excluded_va_ranges.empty());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingVaRangesData)
    {
        json["system"]["value"]["gpus"][0]["memory"]["excludedVaRanges"][0].RemoveMember("base");
        json["system"]["value"]["gpus"][0]["memory"]["excludedVaRanges"][0].RemoveMember("size");
        EXPECT_TRUE(Parse());

        ASSERT_FALSE(parsed_info.system_info.gpus.empty());
        ASSERT_FALSE(parsed_info.system_info.gpus[0].memory.excluded_va_ranges.empty());

        EXPECT_EQ(0, parsed_info.system_info.gpus[0].memory.excluded_va_ranges[0].base);
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].memory.excluded_va_ranges[0].size);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingHeaps)
    {
        json["system"]["value"]["gpus"][0]["memory"].RemoveMember("heaps");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingInvisibleHeap)
    {
        json["system"]["value"]["gpus"][0]["memory"]["heaps"].RemoveMember("invisible");
        EXPECT_TRUE(Parse());

        ASSERT_FALSE(parsed_info.system_info.gpus.empty());
        EXPECT_EQ(1, parsed_info.system_info.gpus[0].memory.heaps.size());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingLocalHeap)
    {
        json["system"]["value"]["gpus"][0]["memory"]["heaps"].RemoveMember("local");
        EXPECT_TRUE(Parse());

        ASSERT_FALSE(parsed_info.system_info.gpus.empty());
        EXPECT_EQ(1, parsed_info.system_info.gpus[0].memory.heaps.size());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingHeapData)
    {
        json["system"]["value"]["gpus"][0]["memory"]["heaps"]["invisible"].RemoveMember("physAddr");
        json["system"]["value"]["gpus"][0]["memory"]["heaps"]["invisible"].RemoveMember("size");

        json["system"]["value"]["gpus"][0]["memory"]["heaps"]["local"].RemoveMember("physAddr");
        json["system"]["value"]["gpus"][0]["memory"]["heaps"]["local"].RemoveMember("size");
        EXPECT_TRUE(Parse());

        ASSERT_FALSE(parsed_info.system_info.gpus.empty());
        ASSERT_EQ(2, parsed_info.system_info.gpus[0].memory.heaps.size());

        EXPECT_EQ(0, parsed_info.system_info.gpus[0].memory.heaps[0].phys_addr);
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].memory.heaps[0].size);

        EXPECT_EQ(0, parsed_info.system_info.gpus[0].memory.heaps[1].phys_addr);
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].memory.heaps[1].size);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingMemClk)
    {
        json["system"]["value"]["gpus"][0]["memory"].RemoveMember("memClockHz");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingMemClkData)
    {
        json["system"]["value"]["gpus"][0]["memory"]["memClockHz"].RemoveMember("max");
        json["system"]["value"]["gpus"][0]["memory"]["memClockHz"].RemoveMember("min");
        EXPECT_TRUE(Parse());

        ASSERT_FALSE(parsed_info.system_info.gpus.empty());
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].memory.mem_clock_hz.max);
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].memory.mem_clock_hz.min);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingMemoryData)
    {
        json["system"]["value"]["gpus"][0]["memory"].RemoveMember("bandwidth");
        json["system"]["value"]["gpus"][0]["memory"].RemoveMember("busBitWidth");
        json["system"]["value"]["gpus"][0]["memory"].RemoveMember("memOpsPerClock");
        json["system"]["value"]["gpus"][0]["memory"].RemoveMember("type");
        EXPECT_TRUE(Parse());

        ASSERT_FALSE(parsed_info.system_info.gpus.empty());
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].memory.bandwidth);
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].memory.bus_bit_width);
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].memory.mem_ops_per_clock);
        EXPECT_EQ("", parsed_info.system_info.gpus[0].memory.type);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingPci)
    {
        json["system"]["value"]["gpus"][0].RemoveMember("pci");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingPciData)
    {
        json["system"]["value"]["gpus"][0]["pci"].RemoveMember("bus");
        json["system"]["value"]["gpus"][0]["pci"].RemoveMember("device");
        json["system"]["value"]["gpus"][0]["pci"].RemoveMember("function");
        EXPECT_TRUE(Parse());

        ASSERT_FALSE(parsed_info.system_info.gpus.empty());
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].pci.bus);
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].pci.device);
        EXPECT_EQ(0, parsed_info.system_info.gpus[0].pci.function);
    }

    // OS Tests
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    TEST_F(SystemInfoParserV3Test, TestMissingOs)
    {
        json["system"]["value"].RemoveMember("os");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingOsMemory)
    {
        json["system"]["value"]["os"].RemoveMember("memory");
        EXPECT_TRUE(Parse());
    }

    TEST_F(SystemInfoParserV3Test, TestMissingOsMemoryData)
    {
        json["system"]["value"]["os"]["memory"].RemoveMember("physical");
        json["system"]["value"]["os"]["memory"].RemoveMember("swap");
        EXPECT_TRUE(Parse());

        EXPECT_EQ(0, parsed_info.system_info.os_info.memory.physical);
        EXPECT_EQ(0, parsed_info.system_info.os_info.memory.swap);
    }

    TEST_F(SystemInfoParserV3Test, TestMissingOsData)
    {
        json["system"]["value"]["os"].RemoveMember("desc");
        json["system"]["value"]["os"].RemoveMember("hostname");
        json["system"]["value"]["os"].RemoveMember("name");
        json["system"]["value"]["os"].RemoveMember("type");

        EXPECT_TRUE(Parse());

        EXPECT_EQ("", parsed_info.system_info.os_info.desc);
        EXPECT_EQ("", parsed_info.system_info.os_info.hostname);
        EXPECT_EQ("", parsed_info.system_info.os_info.name);
    }

};  // namespace devtrace
