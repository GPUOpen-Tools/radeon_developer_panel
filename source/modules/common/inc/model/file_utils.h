// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for file IO utilities

#ifndef RDP_SOURCE_MODULES_COMMON_INC_MODEL_FILE_UTILS_H_
#define RDP_SOURCE_MODULES_COMMON_INC_MODEL_FILE_UTILS_H_

#include <QString>

/// @brief A set of utilities for manipulating the file system.
class FileUtils
{
public:
    /// @brief Destructor.
    virtual ~FileUtils() = default;

    /// @brief Removes the file at the specified path.
    /// @param [in] path The path of the file that should be removed from disk.
    virtual void RemoveFile(const QString& path) const = 0;

    /// @brief Creates a folder at the specified path.
    /// @param [in] path The path of the folder to create.
    virtual void CreateFolder(const QString& path) const = 0;

    /// @brief Verifies that the given directory is valid to output files to.
    /// @param [in] path The output directory path
    /// @param [in] keep_directory True to keep created directory
    /// @param [in] show_alerts true if alerts should be shown, false otherwise.
    virtual bool VerifyOutputDirectory(const QString& path, bool keep_directory, bool show_alerts) const = 0;
};

/// @brief Implementation of the FileUtils interface using Qt.
class QtFileUtils : public FileUtils
{
public:
    /// @brief Destructor.
    ~QtFileUtils() override = default;

    /// @brief Removes the file at the specified path.
    /// @param [in] path The path of the file that should be removed from disk.
    void RemoveFile(const QString& path) const override;

    /// @brief Creates a folder at the specified path.
    /// @param [in] path The path of the folder to create.
    void CreateFolder(const QString& path) const override;

    /// @brief Verifies that the given directory is valid to output files to.
    /// @param [in] path The output directory path
    /// @param [in] keep_directory True to keep created directory
    /// @param [in] show_alerts true if alerts should be shown, false otherwise.
    bool VerifyOutputDirectory(const QString& path, bool keep_directory, bool show_alerts) const override;
};

#endif
