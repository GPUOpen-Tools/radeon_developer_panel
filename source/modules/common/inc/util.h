// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Common utility functions

#ifndef RDP_SOURCE_MODULES_COMMON_INC_UTIL_H_
#define RDP_SOURCE_MODULES_COMMON_INC_UTIL_H_

#include <cstdint>

#include <QDateTime>
#include <QDir>
#include <QString>
#include <QTableView>
#include <QTreeView>
#include <QWidget>

class Util
{
public:
    static constexpr char const* kSettingsDirectory = "RadeonDeveloperDriver";  ///< RDP and RDS use this directory to write settings to.

    /// @brief Gets the RDP main window from widget hierarchy
    /// @return RDP main window widget
    static QWidget* GetRDPMainWindow();

    /// Handles opening a system file dialog and selecting file
    ///
    /// \param path The filepath to select
    static void BrowseToFile(const QString& path);

    /// Serialize memory buffer to disk
    /// \param data The memory buffer
    /// \param offset The offset into buffer
    /// \param size The size of buffer
    /// \param filepath The output file path
    /// \return true if successful, false otherwise
    static bool SerializeMemoryBuffer(const uint8_t* data, uint32_t offset, uint32_t size, const QString& filepath);

    /// Serialize memory buffer to disk
    /// \param data The memory buffer
    /// \param offset The offset into buffer
    /// \param size The size of buffer
    /// \param root The root directory
    /// \param filename The output file name
    /// \return true if successful, false otherwise
    static bool SerializeMemoryBuffer(const uint8_t* data, uint32_t offset, uint32_t size, const QString& root, const QString& filename);

    /// Deserialize memory buffer from file
    /// \param dst destination buffer
    /// \param src file path
    /// \param read number of bytes to read
    static void DeserializeMemoryBuffer(uint8_t* dst, const QString& src, uint64_t read);

    /// Get a string representation of a date that follows OS format
    /// \param dt The date time to convert
    /// \param format The format to apply
    /// \return a string representation of
    static QString GetSystemDate(const QDateTime& dt, const QString& format);

    /// Copy a file
    /// \param src The source file
    /// \param dst The destination file
    /// \param overwrite Allow overwritting existing files
    /// \return true if successful, false otherwise
    static bool CopyFile(const QString& src, const QString& dst, bool overwrite);

    /// @brief Get the location on disk for the DriverTools settings with the specified
    /// folder name, creating it if it doesn't exists.
    ///
    /// On Windows this will be AppData/Roaming/folder_name
    /// and on Unix systems it will be ~/.folder_name.
    /// @param folder_name The name of the folder.
    /// @return A path to the folder.
    static QString GetSettingsFolder(const QString& folder_name);

    /// @brief Verifies that the given directory is valid to output files to.
    /// @param [in] path The output directory path
    /// @param [in] keep_directory True to keep created directory
    /// @param [in] show_alerts true if alerts should be shown, false otherwise.
    static bool VerifyOutputDirectory(const QString& path, bool keep_directory = false, bool show_alerts = true);

    /// @brief Attempts to create a writable temporary file to check for write permissions.
    /// @param [in] dir The directory to check for write permissions
    /// @return true if writable, false otherwise
    static bool IsDirectoryWritable(const QDir& dir);

    /// @brief Gets the default output folder for dumping traces for a module.
    /// @param [in] default_trace_output_folder_name The name of the folder where traces should be dumped.
    /// @return The default output path for a module dumping traces.
    static QString GetDefaultOutputPath(const QString& default_trace_output_folder_name);

    /// @brief Gets the path to the file specified but with a different extension.
    /// @param [in] path The path to the file to get with a different extension.
    /// @param [in] new_extension The new extension of the file.
    /// @return The path to the specified file but with the new extension.
    static QString GetPathForFileWithDifferentExtension(const QString& path, const QString& new_extension);

    /// @brief Expands all of the macros in the output path.
    /// @param [in] path The path to expand.
    /// @param [in] application_name The application to get the expanded output path for.
    /// @return The expanded output path .
    static QString ExpandOutputPathMacros(const QString& path, const QString& application_name);

    /// @brief Sets common QTableView properties such as column/row size and style.
    /// @param [in] table_view The table view to set properties of.
    static void SetCommonQTableViewProperties(QTableView* table_view);

    /// @brief Set common QTreeView properties such as column/row size and style.
    /// @param [in] tree_view The tree view to set properties of.
    static void SetCommonQTreeViewProperties(QTreeView* tree_view);

private:
    /// @brief Removes the application and API macros from the path.
    /// @param [in] path The path to remove the macros from.
    /// @return The path with the macros removed.
    static QString RemoveDirMacros(const QString& path);
};

#endif
