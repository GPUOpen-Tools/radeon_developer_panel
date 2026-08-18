// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Utility function definitions for RDP

#ifndef RDP_SOURCE_FRONTEND_UTILITIES_H_
#define RDP_SOURCE_FRONTEND_UTILITIES_H_

#include <cstddef>
#include <cstdint>

#include <QDir>

#include <qt_common/utils/qt_util.h>

namespace rdp
{
    namespace util
    {
        /// @brief Platform-independent mechanism for retrieving actual OS thread ID
        uint64_t GetCurrentThreadIdentifier();

        /// @brief Query the name of the calling thread
        /// @param [out] buffer The name buffer
        /// @param [in] size The output buffer size
        void GetCurrentThreadName(char* buffer, size_t size);

        /// @brief Modify the name of the calling thread
        /// @param [in] name The new thread name
        void SetCurrentThreadName(const char* name);

        /// @brief Output a string with a newline to an attached debugger
        /// @param [in] message The message string
        void DebugPrintLine(const char* message);

        /// @brief Provides the directory for the app data folder, creating if it does not exist.
        /// @return The app data directory.
        QDir GetApplicationDataPath();

        /// @brief Provides a formatted version of the current OS name.
        /// @return A pretty formatted version of the OS name.
        QString PrettyOsName();
    }  // namespace util

}  // namespace rdp

#endif
