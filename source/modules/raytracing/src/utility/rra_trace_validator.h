// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for a validator to make sure that an RRA trace has BVH data.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_MODEL_UTILITY_RRA_TRACE_VALIDATOR_H_
#define RDP_SOURCE_MODULES_COMMON_INC_MODEL_UTILITY_RRA_TRACE_VALIDATOR_H_

#include <cstdint>

#include <QString>

enum class RraTraceValidationResult : uint8_t
{
    kSuccess = 0,           ///< The trace file was able to be validated and validated successfully.
    kFailedToValidate,      ///< The validity of the trace file was unable to be checked.
    kMissingBvh,            ///< The trace file was missing BVH data.
    kMissingRayHistory,     ///< The trace file was missing ray history data.
    kIncompleteRayHistory,  ///< The trace file had at least one incomplete ray history data.
};

/// @brief Validates whether or not RRA traces have BHV data.
class RraTraceValidator final
{
public:
    /// @brief Validates whether or not an RRA file at the path contains BVH data.
    /// @param [in] file_path The path of the file to validate.
    /// @param [in] validate_ray_history true if ray history should be validate, false otherwise.
    /// @return true if the file was successfully validated, false otherwise.
    [[nodiscard]] static RraTraceValidationResult ValidateFile(const QString& file_path, bool validate_ray_history);
};

#endif
