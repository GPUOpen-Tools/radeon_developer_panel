// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration the handling of opening trace file in external tools.

#include "model/trace_file_opener.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>

TraceFileOpenerResult QProcessTraceFileOpener::Open(const QString& file_path, const QString& exe_path)
{
    if (exe_path.isEmpty())
    {
        return TraceFileOpenerResult::kMissingExecutable;
    }

    QFileInfo exe_info(exe_path);
    if (!exe_info.exists() || !exe_info.isExecutable())
    {
        return TraceFileOpenerResult::kMissingExecutable;
    }

    QStringList args = {file_path};
    QProcess    process;

#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* create_process_args) { create_process_args->inheritHandles = false; });
#endif

    process.setProgram(exe_path);
    process.setArguments(args);

    if (!process.startDetached())
    {
        return TraceFileOpenerResult::kFailedToLaunch;
    }

    return TraceFileOpenerResult::kSuccess;
}
