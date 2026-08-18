// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Command line parameter class implementation

#include "command_line_parameter.h"

#include <cassert>
#include <cstdint>
#include <limits>

#ifndef WIN32
#include <cerrno>
#endif

namespace
{
    constexpr int kDecimalRadix = 10;
}

CommandLineParameter::CommandLineParameter(const char* name, const char* description, const bool required, const Type type, const char* default_value)
    : type_(type)
    , parsed_(false)
    , valid_(false)
    , required_(required)
{
    assert(name != nullptr);
    name_ = name;

    if (description != nullptr)
    {
        description_ = description;
    }

    if (default_value != nullptr)
    {
        value_ = default_value;
    }
}

bool CommandLineParameter::Parse(const std::string& value)
{
    parsed_ = true;

    if (IsFlag())
    {
        valid_ = true;
    }
    else if (!value.empty())
    {
        value_ = value;
        valid_ = true;
    }
    return valid_;
}

const std::string& CommandLineParameter::Value() const
{
    return value_;
}

bool CommandLineParameter::IsValid() const
{
    return valid_;
}

bool CommandLineParameter::IsParsed() const
{
    return parsed_;
}

bool CommandLineParameter::IsFlag() const
{
    return type_ == Type::kFlag;
}

bool CommandLineParameter::IsRequired() const
{
    return required_;
}

const std::string& CommandLineParameter::GetDescription() const
{
    return description_;
}

const std::string& CommandLineParameter::GetName() const
{
    return name_;
}

Int16CommandLineParameter::Int16CommandLineParameter(const char* name, const char* description, const bool required, const int default_value)
    : CommandLineParameter(name, description, required, Type::kValue)
    , int_value_(default_value)
{
}

bool Int16CommandLineParameter::Parse(const std::string& value)
{
    parsed_ = true;
    valid_  = false;
    if (value.empty())
    {
        valid_ = false;
    }
    else
    {
        const char* value_end  = value.c_str() + value.length();
        char*       string_end = nullptr;

        // Convert the string to a base 10 integer value.
        if (const auto port = strtol(value.c_str(), &string_end, kDecimalRadix);
            errno != ERANGE && string_end == value_end && port >= 1 && port <= std::numeric_limits<int16_t>::max())
        {
            int_value_ = static_cast<int>(port);
            valid_     = true;
        }
    }
    return valid_;
}

int Int16CommandLineParameter::GetIntValue() const
{
    return int_value_;
}
