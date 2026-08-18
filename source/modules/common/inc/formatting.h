// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for string formatting utilities.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_FORMATTING_H_
#define RDP_SOURCE_MODULES_COMMON_INC_FORMATTING_H_

#include <array>
#include <cstdint>

#include <QString>

/// @brief Class that contains formatting utilities.
struct Formatting
{
private:
    /// @brief Constructor.
    Formatting() = default;

public:
    /// @brief Provides a string that describes a number of hertz.
    ///
    /// If the value is above 1,000,000 Hz, it will be formatted with the unit in MHz.
    /// If the value is lower than 1MHz, but greater than 1,000Hz, it will be formatted in KHz.
    /// Otherwise the format will just be in Hz.
    /// @param [in] hertz The number of hertz to format.
    /// @return The formatted string.
    static QString FormatHertz(uint64_t hertz, int decimals = 0);

    /// @brief Provides a string converted from Hz into MHz.
    /// @param [in] hertz The number of hertz to format.
    /// @return The formatted string.
    static QString HzToMHz(uint64_t hertz);

    /// @brief Provides a string converted from MHz into GHz.
    /// @param [in] m_hertz The number of hertz to format.
    /// @return The formatted string.
    static QString MHzToGHz(uint64_t m_hertz);

    /// @brief Provides a string that describes the number of bytes.
    ///
    /// The string will use the largest unit that accurately describes the number of bytes.
    /// This function will use the power of 2 measurements, so GiB, KiB, Mib, etc..
    /// @param [in] bytes The number of bytes to format.
    /// @param [in] num_decimals The number of decimal places to use. If it is desired that the
    ///                          number be rounded, 0 should be specified here.
    /// @return The formatted string describing the number of bytes.
    static QString FormatBytesPow2(uint64_t bytes, uint8_t num_decimals = 3);

    /// @brief Provides a string that describes the bandwidth.
    ///
    /// The string will use the largest unit that accurately describes the number of bytes.
    /// This function will use the power of 2 measurements, so GB/s, KB/s, Mb/s, etc..
    /// @param [in] bytes_per_second The number of bytes per second.
    /// @param [in] num_decimals The number of decimal places to use. If it is desired that the
    ///                          number be rounded, 0 should be specified here.
    /// @return The formatted string describing the number of bytes per second.
    static QString FormatBandwidth(uint64_t bytes_per_second, uint8_t num_decimals = 3);

private:
    /// @brief Formats a number by bucketing it based on the highest power of exp_base that the number fits into.
    /// @tparam LevelCount The number of levels to break the number into.
    /// @param number The number for format.
    /// @param exp_base The base for the exponent that determines the unit buckets.
    /// @param labels The labels for each bucket. The formula for the index is number < exp_base^{index - 1}.
    /// @param num_decimals The number of decimals to include in the formatted string.
    /// @return The number formatted as a string.
    template <size_t LevelCount>
    static QString Format(uint64_t number, uint16_t exp_base, const std::array<const char*, LevelCount>& labels, uint8_t num_decimals = 3);
};

#endif
