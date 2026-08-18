// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Input validation utilities implementation.

#include "../inc/input_validation.h"

#include <limits>

namespace InputValidation
{
    template <typename T>
    T ConvertString(const QString& str, int base, bool* ok)
    {
        if constexpr (std::is_same_v<T, uint64_t>)
        {
            return str.toULongLong(ok, base);
        }
        else if constexpr (std::is_same_v<T, int64_t>)
        {
            return str.toLongLong(ok, base);
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            return str.toUInt(ok, base);
        }
        else if constexpr (std::is_same_v<T, int32_t>)
        {
            return str.toInt(ok, base);
        }
        else if constexpr (std::is_same_v<T, uint16_t>)
        {
            uint32_t temp = str.toUInt(ok, base);
            if (*ok && temp <= std::numeric_limits<uint16_t>::max())
            {
                return static_cast<T>(temp);
            }
            *ok = false;
            return T{};
        }
        else if constexpr (std::is_same_v<T, int16_t>)
        {
            int32_t temp = str.toInt(ok, base);
            if (*ok && temp >= std::numeric_limits<int16_t>::min() && temp <= std::numeric_limits<int16_t>::max())
            {
                return static_cast<T>(temp);
            }
            *ok = false;
            return T{};
        }
        else if constexpr (std::is_same_v<T, uint8_t>)
        {
            uint32_t temp = str.toUInt(ok, base);
            if (*ok && temp <= std::numeric_limits<uint8_t>::max())
            {
                return static_cast<T>(temp);
            }
            *ok = false;
            return T{};
        }
        else if constexpr (std::is_same_v<T, int8_t>)
        {
            int32_t temp = str.toInt(ok, base);
            if (*ok && temp >= std::numeric_limits<int8_t>::min() && temp <= std::numeric_limits<int8_t>::max())
            {
                return static_cast<T>(temp);
            }
            *ok = false;
            return T{};
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            // For floating point, only support decimal (base 10)
            if (base != 10)
            {
                *ok = false;
                return T{};
            }
            return str.toDouble(ok);
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            // For floating point, only support decimal (base 10)
            if (base != 10)
            {
                *ok = false;
                return T{};
            }
            return str.toFloat(ok);
        }
        else
        {
            static_assert(sizeof(T) == 0, "Unsupported type for ConvertString");
            *ok = false;
            return T{};
        }
    }

    template <typename T>
    ValidationResult
    ValidateSpinBoxInput(const QString& input, int& position, T min_value, T max_value, bool use_hex_mode, bool allow_optional, QString& last_acceptable_text)
    {
        ValidationResult result;
        result.modified_input = input;
        result.position_delta = 0;

        // Handle empty input
        if (input.isEmpty())
        {
            if (allow_optional)
            {
                last_acceptable_text = input;
                result.state         = QValidator::Acceptable;
                return result;
            }
            result.state = QValidator::Intermediate;
            return result;
        }

        // Check if input starts with "0x" or "0X" (hex input)
        bool is_hex_input = input.startsWith("0x", Qt::CaseInsensitive);

        // Floating point types don't support hex input
        if constexpr (std::is_floating_point_v<T>)
        {
            if (is_hex_input || use_hex_mode)
            {
                result.state = QValidator::Invalid;
                return result;
            }
        }

        if (is_hex_input)
        {
            if (input.length() == 2)  // Just "0x"
            {
                result.state = QValidator::Intermediate;
                return result;
            }

            // Try to parse as hex directly
            bool    ok       = false;
            QString hex_part = input.mid(2);
            T       value    = ConvertString<T>(hex_part, 16, &ok);

            if (!ok)
            {
                result.state = QValidator::Invalid;
                return result;
            }

            if (value <= max_value && value >= min_value)
            {
                last_acceptable_text = input;
                result.state         = QValidator::Acceptable;
                return result;
            }

            result.state = QValidator::Invalid;
            return result;
        }

        // Handle hex mode when input doesn't start with 0x
        if (use_hex_mode)
        {
            if (input == "0")
            {
                result.state = QValidator::Intermediate;
                return result;
            }

            if (!input.startsWith("0x"))
            {
                const QString modified_input = "0x" + input;

                bool ok = false;
                ConvertString<T>(input, 16, &ok);

                if (!ok)
                {
                    result.state = QValidator::Invalid;
                    return result;
                }

                result.position_delta += (modified_input.size() - input.size());
                result.modified_input = modified_input;
                position += result.position_delta;
            }
        }
        else
        {
            // Handle negative input for decimal mode
            if (min_value < 0 && input == "-")
            {
                result.state = QValidator::Intermediate;
                return result;
            }

            // Handle decimal point for floating point types
            if constexpr (std::is_floating_point_v<T>)
            {
                if (input == "." || input == "-.")
                {
                    result.state = QValidator::Intermediate;
                    return result;
                }
            }
        }

        // Convert and validate range
        bool    ok        = false;
        int     base      = is_hex_input ? 16 : (use_hex_mode ? 16 : 10);
        QString value_str = is_hex_input ? input.mid(2) : input;
        T       value     = ConvertString<T>(value_str, base, &ok);

        if (!ok)
        {
            result.state = QValidator::Invalid;
            return result;
        }

        if (value <= max_value && value >= min_value)
        {
            last_acceptable_text = result.modified_input;
            result.state         = QValidator::Acceptable;
            return result;
        }

        result.state = QValidator::Invalid;
        return result;
    }

    // Explicit template instantiations for common types
    template ValidationResult ValidateSpinBoxInput<uint8_t>(const QString&, int&, uint8_t, uint8_t, bool, bool, QString&);
    template ValidationResult ValidateSpinBoxInput<int8_t>(const QString&, int&, int8_t, int8_t, bool, bool, QString&);
    template ValidationResult ValidateSpinBoxInput<uint16_t>(const QString&, int&, uint16_t, uint16_t, bool, bool, QString&);
    template ValidationResult ValidateSpinBoxInput<int16_t>(const QString&, int&, int16_t, int16_t, bool, bool, QString&);
    template ValidationResult ValidateSpinBoxInput<uint32_t>(const QString&, int&, uint32_t, uint32_t, bool, bool, QString&);
    template ValidationResult ValidateSpinBoxInput<int32_t>(const QString&, int&, int32_t, int32_t, bool, bool, QString&);
    template ValidationResult ValidateSpinBoxInput<uint64_t>(const QString&, int&, uint64_t, uint64_t, bool, bool, QString&);
    template ValidationResult ValidateSpinBoxInput<int64_t>(const QString&, int&, int64_t, int64_t, bool, bool, QString&);
    template ValidationResult ValidateSpinBoxInput<float>(const QString&, int&, float, float, bool, bool, QString&);
    template ValidationResult ValidateSpinBoxInput<double>(const QString&, int&, double, double, bool, bool, QString&);

    template uint8_t  ConvertString<uint8_t>(const QString&, int, bool*);
    template int8_t   ConvertString<int8_t>(const QString&, int, bool*);
    template uint16_t ConvertString<uint16_t>(const QString&, int, bool*);
    template int16_t  ConvertString<int16_t>(const QString&, int, bool*);
    template uint32_t ConvertString<uint32_t>(const QString&, int, bool*);
    template int32_t  ConvertString<int32_t>(const QString&, int, bool*);
    template uint64_t ConvertString<uint64_t>(const QString&, int, bool*);
    template int64_t  ConvertString<int64_t>(const QString&, int, bool*);
    template float    ConvertString<float>(const QString&, int, bool*);
    template double   ConvertString<double>(const QString&, int, bool*);

}  // namespace InputValidation
