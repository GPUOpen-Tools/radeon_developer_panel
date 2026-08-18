// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for file IO utilities

#include "common/inc/model/file_utils.h"

#include <QDir>
#include <QFile>

#include "common/inc/util.h"

void QtFileUtils::RemoveFile(const QString& path) const
{
    QFile file(path);
    if (file.exists())
    {
        file.remove();
    }
}

void QtFileUtils::CreateFolder(const QString& path) const
{
    QDir directory(path);
    directory.mkpath(".");
}

bool QtFileUtils::VerifyOutputDirectory(const QString& path, bool keep_directory, bool show_alerts) const
{
    return Util::VerifyOutputDirectory(path, keep_directory, show_alerts);
}
