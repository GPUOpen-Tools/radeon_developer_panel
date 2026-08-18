//=============================================================================
// Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  The definition for Driver Settings Errors.
//=============================================================================
#ifndef RDP_SOURCE_MODULES_COMMON_INC_API_MODULE_DRIVER_SETTING_ERRORS_H_
#define RDP_SOURCE_MODULES_COMMON_INC_API_MODULE_DRIVER_SETTING_ERRORS_H_

#include <cstdint>
#include <ostream>
#include <string_view>

/// @brief Result codes for loading module userdata.
/// Extend only when a new category is needed across multiple modules.
enum class DriverSettingsErrorType : uint8_t
{
    kSuccess,         ///< No error.
    kParseFailed,     ///< Vague error while parsing JSON.
    kNotInitialized,  ///< Some resource is not initialized.
    kInvalidData,     ///< Invalid input data.
    // Add more errors here if needed
    // NOTE: Keep this last.
    kUnknownError,  ///< An unknown error occurred.
};

constexpr std::string_view DriverSettingsErrorTypeToString(DriverSettingsErrorType e) noexcept
{
    switch (e)
    {
    case DriverSettingsErrorType::kSuccess:
        return "kSuccess";
    case DriverSettingsErrorType::kParseFailed:
        return "kParseFailed";
    case DriverSettingsErrorType::kNotInitialized:
        return "kNotInitialized";
    case DriverSettingsErrorType::kInvalidData:
        return "kInvalidData";
    case DriverSettingsErrorType::kUnknownError:
        return "kUnknownError";
    }
    return "kUnknownError";
}

constexpr std::string_view DriverSettingsErrorTypeErrorMessage(DriverSettingsErrorType e) noexcept
{
    switch (e)
    {
    case DriverSettingsErrorType::kSuccess:
        return "Driver settings loaded successfully.";
    case DriverSettingsErrorType::kParseFailed:
        return "Failed to parse driver settings JSON. The file may be corrupted or incomplete.";
    case DriverSettingsErrorType::kNotInitialized:
        return "Driver settings system is not initialized. Ensure initialization before loading.";
    case DriverSettingsErrorType::kInvalidData:
        return "Driver settings contain invalid or unsupported data. Check the input format.";
    case DriverSettingsErrorType::kUnknownError:
        return "An unknown error occurred while loading driver settings.";
    }
    return "An unknown error occurred while loading driver settings.";
}

#endif
