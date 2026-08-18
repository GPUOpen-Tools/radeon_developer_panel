// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Unit tests for input validation utilities.

#include <common/inc/input_validation.h>

#include <gtest/gtest.h>

using namespace InputValidation;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class InputValidationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        last_acceptable_text_.clear();
    }

    QString last_acceptable_text_;
};

// Tests for uint32_t decimal mode
TEST_F(InputValidationTest, ValidateDecimalUint32_ValidInput)
{
    QString input    = "123";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<uint32_t>(input, position, 0, 1000, false, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Acceptable);
    EXPECT_EQ(result.modified_input, "123");
    EXPECT_EQ(result.position_delta, 0);
    EXPECT_EQ(last_acceptable_text_, "123");
}

TEST_F(InputValidationTest, ValidateDecimalUint32_EmptyWithOptional)
{
    QString input    = "";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<uint32_t>(input, position, 0, 1000, false, true, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Acceptable);
    EXPECT_EQ(result.modified_input, "");
    EXPECT_EQ(result.position_delta, 0);
    EXPECT_EQ(last_acceptable_text_, "");
}

TEST_F(InputValidationTest, ValidateDecimalUint32_EmptyWithoutOptional)
{
    QString input    = "";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<uint32_t>(input, position, 0, 1000, false, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Intermediate);
}

TEST_F(InputValidationTest, ValidateDecimalUint32_OutOfRange)
{
    QString input    = "2000";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<uint32_t>(input, position, 0, 1000, false, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Invalid);
}

TEST_F(InputValidationTest, ValidateDecimalUint32_NegativeForSigned)
{
    QString input    = "-";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<int32_t>(input, position, -100, 1000, false, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Intermediate);
}

// Tests for hex mode
TEST_F(InputValidationTest, ValidateHexUint32_ValidHexInput)
{
    QString input    = "0xFF";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<uint32_t>(input, position, 0, 1000, false, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Acceptable);
    EXPECT_EQ(result.modified_input, "0xFF");
    EXPECT_EQ(result.position_delta, 0);
    EXPECT_EQ(last_acceptable_text_, "0xFF");
}

TEST_F(InputValidationTest, ValidateHexUint32_JustPrefix)
{
    QString input    = "0x";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<uint32_t>(input, position, 0, 1000, false, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Intermediate);
}

TEST_F(InputValidationTest, ValidateHexUint32_InvalidHexDigit)
{
    QString input    = "0xGG";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<uint32_t>(input, position, 0, 1000, false, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Invalid);
}

TEST_F(InputValidationTest, ValidateHexUint32_HexOutOfRange)
{
    QString input    = "0x7D0";  // 2000 in hex, outside range 0-1000
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<uint32_t>(input, position, 0, 1000, false, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Invalid);
}

// Tests for hex mode spinbox
TEST_F(InputValidationTest, ValidateHexModeUint32_AutoPrefixing)
{
    QString input    = "FF";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<uint32_t>(input, position, 0, 1000, true, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Acceptable);
    EXPECT_EQ(result.modified_input, "0xFF");
    EXPECT_EQ(result.position_delta, 2);  // Added "0x"
    EXPECT_EQ(last_acceptable_text_, "0xFF");
}

TEST_F(InputValidationTest, ValidateHexModeUint32_Zero)
{
    QString input    = "0";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<uint32_t>(input, position, 0, 1000, true, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Intermediate);
}

TEST_F(InputValidationTest, ValidateHexModeUint32_AlreadyPrefixed)
{
    QString input    = "0xAB";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<uint32_t>(input, position, 0, 1000, true, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Acceptable);
    EXPECT_EQ(result.modified_input, "0xAB");
    EXPECT_EQ(result.position_delta, 0);
    EXPECT_EQ(last_acceptable_text_, "0xAB");
}

// Tests for ConvertString function
TEST(ConvertStringTest, ConvertUint32Decimal)
{
    bool ok     = false;
    auto result = ConvertString<uint32_t>("123", 10, &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(result, 123u);
}

TEST(ConvertStringTest, ConvertUint32Hex)
{
    bool ok     = false;
    auto result = ConvertString<uint32_t>("FF", 16, &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(result, 255u);
}

TEST(ConvertStringTest, ConvertInt32Negative)
{
    bool ok     = false;
    auto result = ConvertString<int32_t>("-123", 10, &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(result, -123);
}

TEST(ConvertStringTest, ConvertUint8Valid)
{
    bool ok     = false;
    auto result = ConvertString<uint8_t>("255", 10, &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(result, 255);
}

TEST(ConvertStringTest, ConvertUint8Invalid)
{
    bool ok = false;
    ConvertString<uint8_t>("256", 10, &ok);
    EXPECT_FALSE(ok);
}

TEST(ConvertStringTest, ConvertInvalidString)
{
    bool ok = false;
    ConvertString<uint32_t>("invalid", 10, &ok);
    EXPECT_FALSE(ok);
}

// Tests for floating point types
TEST_F(InputValidationTest, ValidateDouble_DecimalInput)
{
    QString input    = "123.45";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<double>(input, position, 0.0, 1000.0, false, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Acceptable);
    EXPECT_EQ(result.modified_input, "123.45");
    EXPECT_EQ(last_acceptable_text_, "123.45");
}

TEST_F(InputValidationTest, ValidateDouble_RejectsHexInput)
{
    QString input    = "0xFF";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<double>(input, position, 0.0, 1000.0, false, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Invalid);
}

TEST_F(InputValidationTest, ValidateDouble_RejectsHexMode)
{
    QString input    = "123";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<double>(input, position, 0.0, 1000.0, true, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Invalid);
}

TEST_F(InputValidationTest, ValidateDouble_DecimalPointIntermediate)
{
    QString input    = ".";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<double>(input, position, 0.0, 1000.0, false, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Intermediate);
}

TEST_F(InputValidationTest, ValidateDouble_NegativeDecimalPointIntermediate)
{
    QString input    = "-.";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<double>(input, position, -100.0, 1000.0, false, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Intermediate);
}

TEST_F(InputValidationTest, ValidateFloat_DecimalInput)
{
    QString input    = "12.5";
    int     position = 0;
    auto    result   = ValidateSpinBoxInput<float>(input, position, 0.0f, 100.0f, false, false, last_acceptable_text_);

    EXPECT_EQ(result.state, QValidator::Acceptable);
    EXPECT_EQ(result.modified_input, "12.5");
    EXPECT_EQ(last_acceptable_text_, "12.5");
}

// Tests for ConvertString with floating point
TEST(ConvertStringTest, ConvertDoubleValid)
{
    bool ok     = false;
    auto result = ConvertString<double>("123.45", 10, &ok);
    EXPECT_TRUE(ok);
    EXPECT_DOUBLE_EQ(result, 123.45);
}

TEST(ConvertStringTest, ConvertDoubleHexInvalid)
{
    bool ok = false;
    ConvertString<double>("FF", 16, &ok);
    EXPECT_FALSE(ok);
}

TEST(ConvertStringTest, ConvertFloatValid)
{
    bool ok     = false;
    auto result = ConvertString<float>("12.5", 10, &ok);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(result, 12.5f);
}
