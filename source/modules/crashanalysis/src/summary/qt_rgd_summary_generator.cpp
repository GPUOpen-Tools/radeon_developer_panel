// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for an object that can generate RGD summaries done with QProcess.

#include "qt_rgd_summary_generator.h"

#include <QProcess>
#include <QSettings>

#include <common/inc/util.h>

static constexpr int kProcessFinishWaitTimeMs = 50;
static constexpr int kProcessKillWaitTimeMs   = 500;

QtRgdSummaryGenerator::QtRgdSummaryGenerator(QSettings* tool_settings)
    : tool_settings_(tool_settings)
{
}

devtrace::Result QtRgdSummaryGenerator::GenerateSummaries(const std::string&                 dump_path_utf8,
                                                          const bool                         generate_text,
                                                          const bool                         generate_json,
                                                          std::string&                       error,
                                                          std::string&                       text_path,
                                                          std::string&                       json_path,
                                                          std::atomic<bool>&                 should_abort,
                                                          const devtrace::RgdSummaryOptions& options)
{
    text_path = "";
    json_path = "";

    if (!generate_text && !generate_json)
    {
        return devtrace::Result::kSuccess;
    }

    const QString dump_path = QDir::toNativeSeparators(QString::fromStdString(dump_path_utf8));
    if (const QFileInfo file_info(dump_path); !file_info.exists())
    {
        return devtrace::Result::kFailure;
    }

    const QString tool_path = GetToolPath();
    if (tool_path.isEmpty())
    {
        return devtrace::Result::kFailure;
    }

    if (const QFileInfo tool_file_info(tool_path); !tool_file_info.exists())
    {
        return devtrace::Result::kExecutableNotFound;
    }

    devtrace::Result result = devtrace::Result::kSuccess;

    if (generate_text)
    {
        QStringList text_args = {"--parse", dump_path};
        CreateArgs(true, false, dump_path, options, text_args, text_path, json_path);

        const devtrace::Result text_result = RunProcess(tool_path, text_args, should_abort);
        if (text_result != devtrace::Result::kSuccess)
        {
            result = text_result;
        }
        else
        {
            std::string            text_error;
            const devtrace::Result text_validate = DetermineResult(text_path, "", text_error);
            if (text_validate != devtrace::Result::kSuccess)
            {
                result = text_validate;
                error  = text_error;
            }
        }
    }

    if (generate_json)
    {
        std::string unused_text_path;
        QStringList json_args = {"--parse", dump_path};
        CreateArgs(false, true, dump_path, options, json_args, unused_text_path, json_path);

        const devtrace::Result json_result = RunProcess(tool_path, json_args, should_abort);
        if (json_result != devtrace::Result::kSuccess)
        {
            result = json_result;
        }
        else
        {
            std::string            json_error;
            const devtrace::Result json_validate = DetermineResult("", json_path, json_error);
            if (json_validate != devtrace::Result::kSuccess)
            {
                result = json_validate;
                if (error.empty())
                {
                    error = json_error;
                }
                else
                {
                    error += "\n" + json_error;
                }
            }
        }
    }

    return result;
}

devtrace::Result QtRgdSummaryGenerator::RunProcess(const QString& tool_path, const QStringList& args, std::atomic<bool>& should_abort)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(tool_path, args);

    while (!process.waitForFinished(kProcessFinishWaitTimeMs))
    {
        if (should_abort)
        {
            process.kill();
            process.waitForFinished(kProcessKillWaitTimeMs);
            return devtrace::Result::kFailure;
        }
    }

    if (process.exitStatus() == QProcess::CrashExit || process.exitCode() != 0)
    {
        return devtrace::Result::kFailure;
    }

    return devtrace::Result::kSuccess;
}

QString QtRgdSummaryGenerator::GetToolPath() const
{
    if (tool_settings_ == nullptr || !tool_settings_->contains("rgd_path"))
    {
        return "";
    }

    return tool_settings_->value("rgd_path").toString();
}

void QtRgdSummaryGenerator::CreateArgs(const bool                         generate_text,
                                       const bool                         generate_json,
                                       const QString&                     dump_path,
                                       const devtrace::RgdSummaryOptions& options,
                                       QStringList&                       args,
                                       std::string&                       text_path,
                                       std::string&                       json_path)
{
    if (generate_text)
    {
        const QString output_path = Util::GetPathForFileWithDifferentExtension(dump_path, "txt");
        args.append({"--output", output_path});
        text_path = output_path.toStdString();
    }

    if (generate_json)
    {
        const QString output_path = Util::GetPathForFileWithDifferentExtension(dump_path, "json");
        args.append({"--json", output_path});
        json_path = output_path.toStdString();
    }

    if (options.show_marker_source)
    {
        args.push_back("--marker-src");
    }

    if (options.expand_markers)
    {
        args.push_back("--expand-markers");
    }

    if (!options.pdb_search_paths.empty())
    {
        for (const auto& path : options.pdb_search_paths)
        {
            if (!path.empty())
            {
                args.push_back(QString("--pdb-path"));
                args.push_back(path.c_str());
            }
        }
    }

    if (options.pdb_include_subfolders)
    {
        args.push_back("--pdb-subdir");
    }
}

devtrace::Result QtRgdSummaryGenerator::DetermineResult(const std::string& text_path, const std::string& json_path, std::string& error)
{
    const bool text_summary_valid = IsFileValid(text_path);
    const bool json_summary_valid = IsFileValid(json_path);

    if (!text_summary_valid || !json_summary_valid)
    {
        error = "Summary file validation failed.";
        return devtrace::Result::kFailure;
    }

    return devtrace::Result::kSuccess;
}

bool QtRgdSummaryGenerator::IsFileValid(const std::string& path)
{
    if (path.empty())
    {
        return true;
    }

    const QFileInfo file_info(QString::fromStdString(path));
    return file_info.exists() && file_info.size() > 0;
}
