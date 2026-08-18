// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Tests for formatting utilities.

#include <common/inc/formatting.h>

#include <gtest/gtest.h>

// Hertz tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(FormattingTest, TestFormat0Hertz)
{
    EXPECT_EQ("0 Hz", Formatting::FormatHertz(0));
}

TEST(FormattingTest, TestFormat100Hertz)
{
    EXPECT_EQ("100 Hz", Formatting::FormatHertz(100));
}

TEST(FormattingTest, TestFormatKilohertz)
{
    EXPECT_EQ("53 KHz", Formatting::FormatHertz(53420));
}

TEST(FormattingTest, TestFormatMegahertz)
{
    EXPECT_EQ("65 MHz", Formatting::FormatHertz(65000000));
}

TEST(FormattingTest, TestFormatGigahertz)
{
    EXPECT_EQ("3 GHz", Formatting::FormatHertz(2500000000));
}

// Bytes tests (rounded)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(FormattingTest, TestFormat0BytesRound)
{
    EXPECT_EQ("0 B", Formatting::FormatBytesPow2(0, 0));
}

TEST(FormattingTest, TestFormat256BytesRound)
{
    EXPECT_EQ("256 B", Formatting::FormatBytesPow2(256, 0));
}

TEST(FormattingTest, TestFormat1024BytesRound)
{
    EXPECT_EQ("1 KB", Formatting::FormatBytesPow2(1024, 0));
}

TEST(FormattingTest, TestFormat1536BytesRound)
{
    EXPECT_EQ("2 KB", Formatting::FormatBytesPow2(1536, 0));
}

TEST(FormattingTest, TestFormat2063BytesRound)
{
    EXPECT_EQ("2 KB", Formatting::FormatBytesPow2(2063, 0));
}

TEST(FormattingTest, TestFormat100KbRound)
{
    EXPECT_EQ("100 KB", Formatting::FormatBytesPow2(102400, 0));
}

TEST(FormattingTest, TestFormat1MbRound)
{
    EXPECT_EQ("1 MB", Formatting::FormatBytesPow2(1048576, 0));
}

TEST(FormattingTest, TestFormat15MbRound)
{
    EXPECT_EQ("15 MB", Formatting::FormatBytesPow2(16043212, 0));
}

TEST(FormattingTest, TestFormat1GbRound)
{
    EXPECT_EQ("1 GB", Formatting::FormatBytesPow2(1073741824, 0));
}

TEST(FormattingTest, TestFormat100GbRound)
{
    EXPECT_EQ("100 GB", Formatting::FormatBytesPow2(107374182400, 0));
}

TEST(FormattingTest, TestFormat1TbRound)
{
    EXPECT_EQ("1 TB", Formatting::FormatBytesPow2(1099511627776, 0));
}

TEST(FormattingTest, TestFormat13TbRound)
{
    EXPECT_EQ("13 TB", Formatting::FormatBytesPow2(13963797672755, 0));
}

TEST(FormattingTest, TestFormat1PbRound)
{
    EXPECT_EQ("1 PB", Formatting::FormatBytesPow2(1125899906842624, 0));
}

TEST(FormattingTest, TestFormat10PbRound)
{
    EXPECT_EQ("10 PB", Formatting::FormatBytesPow2(11258999068426240, 0));
}

TEST(FormattingTest, TestFormat2000PbRound)
{
    EXPECT_EQ("2000 PB", Formatting::FormatBytesPow2(2251799813685248000, 0));
}

// Bytes tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(FormattingTest, TestFormat0Bytes)
{
    EXPECT_EQ("0 B", Formatting::FormatBytesPow2(0, 2));
}

TEST(FormattingTest, TestFormat256Bytes)
{
    EXPECT_EQ("256 B", Formatting::FormatBytesPow2(256, 2));
}

TEST(FormattingTest, TestFormat1024Bytes)
{
    EXPECT_EQ("1.00 KB", Formatting::FormatBytesPow2(1024, 2));
}

TEST(FormattingTest, TestFormat1536Bytes)
{
    EXPECT_EQ("1.50 KB", Formatting::FormatBytesPow2(1536, 2));
}

TEST(FormattingTest, TestFormat2063Bytes)
{
    EXPECT_EQ("2.01 KB", Formatting::FormatBytesPow2(2063, 2));
}

TEST(FormattingTest, TestFormat100Kb)
{
    EXPECT_EQ("100.00 KB", Formatting::FormatBytesPow2(102400, 2));
}

TEST(FormattingTest, TestFormat1Mb)
{
    EXPECT_EQ("1.00 MB", Formatting::FormatBytesPow2(1048576, 2));
}

TEST(FormattingTest, TestFormat15Mb)
{
    EXPECT_EQ("15.29 MB", Formatting::FormatBytesPow2(16032727, 2));
}

TEST(FormattingTest, TestFormat1Gb)
{
    EXPECT_EQ("1.00 GB", Formatting::FormatBytesPow2(1073741824, 2));
}

TEST(FormattingTest, TestFormat99Gb)
{
    EXPECT_EQ("99.13 GB", Formatting::FormatBytesPow2(106440027013, 2));
}

TEST(FormattingTest, TestFormat1Tb)
{
    EXPECT_EQ("1.00 TB", Formatting::FormatBytesPow2(1099511627776, 2));
}

TEST(FormattingTest, TestFormat13Tb)
{
    EXPECT_EQ("13.94 TB", Formatting::FormatBytesPow2(15327192091197, 2));
}

TEST(FormattingTest, TestFormat1Pb)
{
    EXPECT_EQ("1.00 PB", Formatting::FormatBytesPow2(1125899906842624, 2));
}

TEST(FormattingTest, TestFormat76Pb)
{
    EXPECT_EQ("76.21 PB", Formatting::FormatBytesPow2(85804831900476375, 2));
}

TEST(FormattingTest, TestFormat2000Pb)
{
    EXPECT_EQ("2000.00 PB", Formatting::FormatBytesPow2(2251799813685248000, 2));
}

// Bandwidth tests (rounded)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(FormattingTest, TestFormat0BytessRound)
{
    EXPECT_EQ("0 B/s", Formatting::FormatBandwidth(0, 0));
}

TEST(FormattingTest, TestFormat256BytessRound)
{
    EXPECT_EQ("256 B/s", Formatting::FormatBandwidth(256, 0));
}

TEST(FormattingTest, TestFormat1024BytessRound)
{
    EXPECT_EQ("1 KB/s", Formatting::FormatBandwidth(1000, 0));
}

TEST(FormattingTest, TestFormat1536BytessRound)
{
    EXPECT_EQ("2 KB/s", Formatting::FormatBandwidth(1500, 0));
}

TEST(FormattingTest, TestFormat2063BytessRound)
{
    EXPECT_EQ("2 KB/s", Formatting::FormatBandwidth(2063, 0));
}

TEST(FormattingTest, TestFormat100KbsRound)
{
    EXPECT_EQ("100 KB/s", Formatting::FormatBandwidth(100000, 0));
}

TEST(FormattingTest, TestFormat1MbsRound)
{
    EXPECT_EQ("1 MB/s", Formatting::FormatBandwidth(1000000, 0));
}

TEST(FormattingTest, TestFormat15MbsRound)
{
    EXPECT_EQ("15 MB/s", Formatting::FormatBandwidth(15450000, 0));
}

TEST(FormattingTest, TestFormat1GbsRound)
{
    EXPECT_EQ("1 GB/s", Formatting::FormatBandwidth(1000000000, 0));
}

TEST(FormattingTest, TestFormat100GbsRound)
{
    EXPECT_EQ("100 GB/s", Formatting::FormatBandwidth(100270000000, 0));
}

TEST(FormattingTest, TestFormat1TbsRound)
{
    EXPECT_EQ("1 TB/s", Formatting::FormatBandwidth(1002700000000, 0));
}

TEST(FormattingTest, TestFormat13TbsRound)
{
    EXPECT_EQ("13 TB/s", Formatting::FormatBandwidth(13027000000000, 0));
}

TEST(FormattingTest, TestFormat1PbsRound)
{
    EXPECT_EQ("1 PB/s", Formatting::FormatBandwidth(1000000000000000, 0));
}

TEST(FormattingTest, TestFormat10PbsRound)
{
    EXPECT_EQ("10 PB/s", Formatting::FormatBandwidth(10460000000000000, 0));
}

TEST(FormattingTest, TestFormat2046PbsRound)
{
    EXPECT_EQ("2046 PB/s", Formatting::FormatBandwidth(2046000000000000000, 0));
}

// Bandwidth tests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(FormattingTest, TestFormat0Bytess)
{
    EXPECT_EQ("0 B/s", Formatting::FormatBandwidth(0, 2));
}

TEST(FormattingTest, TestFormat256Bytess)
{
    EXPECT_EQ("256 B/s", Formatting::FormatBandwidth(256, 2));
}

TEST(FormattingTest, TestFormat1024Bytess)
{
    EXPECT_EQ("1.00 KB/s", Formatting::FormatBandwidth(1000, 2));
}

TEST(FormattingTest, TestFormat1536Bytess)
{
    EXPECT_EQ("1.50 KB/s", Formatting::FormatBandwidth(1500, 2));
}

TEST(FormattingTest, TestFormat2063Bytess)
{
    EXPECT_EQ("2.06 KB/s", Formatting::FormatBandwidth(2063, 2));
}

TEST(FormattingTest, TestFormat100Kbs)
{
    EXPECT_EQ("100.00 KB/s", Formatting::FormatBandwidth(100000, 2));
}

TEST(FormattingTest, TestFormat1Mbs)
{
    EXPECT_EQ("1.00 MB/s", Formatting::FormatBandwidth(1000000, 2));
}

TEST(FormattingTest, TestFormat15Mbs)
{
    EXPECT_EQ("15.45 MB/s", Formatting::FormatBandwidth(15450000, 2));
}

TEST(FormattingTest, TestFormat1Gbs)
{
    EXPECT_EQ("1.00 GB/s", Formatting::FormatBandwidth(1000000000, 2));
}

TEST(FormattingTest, TestFormat100Gbs)
{
    EXPECT_EQ("100.27 GB/s", Formatting::FormatBandwidth(100270000000, 2));
}

TEST(FormattingTest, TestFormat1Tbs)
{
    EXPECT_EQ("1.83 TB/s", Formatting::FormatBandwidth(1827000000000, 2));
}

TEST(FormattingTest, TestFormat13Tbs)
{
    EXPECT_EQ("13.03 TB/s", Formatting::FormatBandwidth(13027000000000, 2));
}

TEST(FormattingTest, TestFormat1Pbs)
{
    EXPECT_EQ("1.00 PB/s", Formatting::FormatBandwidth(1000000000000000, 2));
}

TEST(FormattingTest, TestFormat10Pbs)
{
    EXPECT_EQ("10.46 PB/s", Formatting::FormatBandwidth(10460000000000000, 2));
}

TEST(FormattingTest, TestFormat2046Pbs)
{
    EXPECT_EQ("2046.00 PB/s", Formatting::FormatBandwidth(2046000000000000000, 2));
}
