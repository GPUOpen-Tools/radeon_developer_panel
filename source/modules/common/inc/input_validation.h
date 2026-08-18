// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Input validation utilities.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_INPUT_VALIDATION_H_
#define RDP_SOURCE_MODULES_COMMON_INC_INPUT_VALIDATION_H_

#include <cstdint>
#include <optional>
#include <type_traits>

#include <QString>
#include <QValidator>

/// @brief Utilities for input validation.
namespace InputValidation
{
    /// @brief Validation result for spinbox input.
    struct ValidationResult
    {
        QValidator::State state;           ///< The validation state.
        QString           modified_input;  ///< The potentially modified input string.
        int               position_delta;  ///< Change in cursor position.
    };

    /// @brief Validates spinbox input with support for both decimal and hexadecimal values.
    /// @tparam T The integer type (e.g., uint32_t, int64_t).
    /// @param input The input string to validate.
    /// @param position The cursor position (may be modified).
    /// @param min_value The minimum allowed value.
    /// @param max_value The maximum allowed value.
    /// @param use_hex_mode True if the spinbox is in hex mode.
    /// @param allow_optional True if empty input is allowed.
    /// @param last_acceptable_text Reference to store the last acceptable text.
    /// @return ValidationResult containing the state and any modifications.
    template <typename T>
    ValidationResult
    ValidateSpinBoxInput(const QString& input, int& position, T min_value, T max_value, bool use_hex_mode, bool allow_optional, QString& last_acceptable_text);

    /// @brief Converts a string to the specified integer type.
    /// @tparam T The integer type to convert to.
    /// @param str The string to convert.
    /// @param base The numeric base (10 for decimal, 16 for hex).
    /// @param ok Pointer to bool that will be set to true if conversion succeeded.
    /// @return The converted value.
    template <typename T>
    T ConvertString(const QString& str, int base, bool* ok);

}  // namespace InputValidation

#endif
