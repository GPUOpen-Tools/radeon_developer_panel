// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  RGP file validator.

#ifndef RDP_MODULES_PROFILING_RGP_FILE_VALIDATOR_H_
#define RDP_MODULES_PROFILING_RGP_FILE_VALIDATOR_H_

#include <string>

#include <QMetaType>
#include <QString>

/// @brief An enumeration for RGP file validation status
typedef enum RgpFileValidatorStatus
{
    kRgpFileValidatorStatusOk                = 0,  ///< No problems encountered while validating RGP file data.
    kRgpFileValidatorStatusFileReadError     = 1,  ///< File read error encountered while validating RGP file data.
    kRgpFileValidatorStatusParserError       = 2,  ///< Parser error encountered while validating RGP file data.
    kRgpFileValidatorStatusMemoryError       = 3,  ///< Out of memory read error encountered while validating RGP file data.
    kRgpFileValidatorStatusSpmError          = 4,  ///< Spm data validation encountered while validating RGP file data.
    kRgpFileValidatorStatusMissingChunkError = 5   ///< A required chunk was missing from the RGP file.
} RgpFileValidatorStatus;

Q_DECLARE_METATYPE(RgpFileValidatorStatus)

/// @brief Loads and validates the profile data in the .rgp file.
///
/// For legacy RGP files, validates the SPM counter data (non-zero timestamps).
/// For Ubertrace (RDF-based) files, checks that all required chunks are present
/// and that the SPM session (if present) contains at least one timestamp.
///
/// @param [in] file_path The full path of the RGP file to validate.
///
/// @return kRgpFileValidatorStatusOk if the profile data is valid, or a non-zero status indicating the specific failure.
RgpFileValidatorStatus RgpValidateProfileData(const QString& file_path);

#endif
