// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration the handling of opening trace file in external tools.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_MODEL_TRACE_FILE_OPENER_H_
#define RDP_SOURCE_MODULES_COMMON_INC_MODEL_TRACE_FILE_OPENER_H_

#include <QString>

/// @brief The different results for opening a trace file in an external program.
enum class TraceFileOpenerResult
{
    kSuccess,            ///< Opening the trace file was a success.
    kMissingExecutable,  ///< The executable was missing.
    kFailedToLaunch      ///< The external executable failed to launch.
};

/// @brief Provides the means to open a trace file in an external program.
class TraceFileOpener
{
public:
    /// @brief Destructor.
    virtual ~TraceFileOpener() = default;

    /// @brief Opens the file in an external tool.
    /// @param [in] file_path The path of the file to open in an external tool.
    /// @param [in] exe_path The path on disk to the tool executable.
    /// @return The result of opening the file in the external program.
    virtual TraceFileOpenerResult Open(const QString& file_path, const QString& exe_path) = 0;
};

/// @brief A trace file opener that uses QProcess to open trace files.
class QProcessTraceFileOpener : public TraceFileOpener
{
public:
    /// @brief Destructor.
    virtual ~QProcessTraceFileOpener() = default;

    /// @brief Opens the file in an external tool by launching the specified executable.
    /// @param [in] file_path The path of the file to open in an external tool.
    /// @param [in] exe_path The path on disk to the tool executable.
    /// @return The result of opening the file in the external program.
    virtual TraceFileOpenerResult Open(const QString& file_path, const QString& exe_path) override;
};

#endif
