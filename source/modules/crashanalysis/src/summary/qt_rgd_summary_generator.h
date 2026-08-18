// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for an object that can generate RGD summaries done with QProcess.

#ifndef RDP_SOURCE_MODULES_CRASHANALYSIS_SRC_SUMMARY_QT_RGD_SUMMARY_GENERATOR_H_
#define RDP_SOURCE_MODULES_CRASHANALYSIS_SRC_SUMMARY_QT_RGD_SUMMARY_GENERATOR_H_

#include <atomic>
#include <string>

#include <QString>
#include <QStringList>

#include <dev_trace_common.h>
#include <rgd_summary_generator.h>

/// @brief An object that can generate RGD summaries.
class QtRgdSummaryGenerator final : public devtrace::RgdSummaryGenerator
{
public:
    /// @brief Destructor.
    ~QtRgdSummaryGenerator() override = default;

    /// @brief Constructor.
    /// @param [in] tool_settings The settings used for the tool invoking this generator.
    explicit QtRgdSummaryGenerator(class QSettings* tool_settings);

    /// @brief Generates either the text, JSON or both summaries for the dump at the path.
    ///
    /// This operation should be synchronous. The output of the summary should be put in the same folder as the dump.
    /// @param [in] dump_path_utf8 The path of the dump to generate the summary for. UTF-8 encoded.
    /// @param [in] generate_text true if the text summary should be generated, false otherwise.
    /// @param [in] generate_json true if the JSON summary should be generated, false otherwise.
    /// @param [out] error The error string if the generation failed. This should be UTF-8 encoded.
    /// @param [out] text_path The UTF-8 encoded path where the text summary was written (or empty of it was not generated).
    /// @param [out] json_path The UTF-8 encoded path where the JSON summary was written (or empty of it was not generated).
    /// @param [in] should_abort True if summary generation should be aborted.
    /// @param [in] options Additional options for the summary generation.
    /// @return Result::kSuccess if the generation was successful.
    devtrace::Result GenerateSummaries(const std::string&                 dump_path_utf8,
                                       bool                               generate_text,
                                       bool                               generate_json,
                                       std::string&                       error,
                                       std::string&                       text_path,
                                       std::string&                       json_path,
                                       std::atomic<bool>&                 should_abort,
                                       const devtrace::RgdSummaryOptions& options) override;

private:
    /// @brief Gets the path fo the RGD tool.
    /// @return The path to the tool or empty string if it was not found.
    [[nodiscard]] QString GetToolPath() const;

    /// @brief Generates the command line arguments to be passed to the RGD executable to generate summaries.
    /// @param [in] generate_text true if the text summary should be generated, false otherwise.
    /// @param [in] generate_json true if the JSON summary should be generated, false otherwise.
    /// @param [in] dump_path The path of the dump file to generate summaries for.
    /// @param [in] options Additional options for the summary generation.
    /// @param [out] args The command line arguments to be passed to the RGD executable.
    /// @param [out] text_path The UTF-8 encoded path where the text summary should be written (or empty of it was not requested).
    /// @param [out] json_path The UTF-8 encoded path where the JSON summary should be written (or empty of it was not requested).
    static void CreateArgs(bool                               generate_text,
                           bool                               generate_json,
                           const QString&                     dump_path,
                           const devtrace::RgdSummaryOptions& options,
                           QStringList&                       args,
                           std::string&                       text_path,
                           std::string&                       json_path);

    /// @brief Runs the RGD tool process with the given arguments.
    /// @param [in] tool_path The path to the RGD executable.
    /// @param [in] args The command line arguments to pass to the RGD executable.
    /// @param [in] should_abort True if the process should be aborted.
    /// @return Result::kSuccess if the process completed successfully.
    static devtrace::Result RunProcess(const QString& tool_path, const QStringList& args, std::atomic<bool>& should_abort);

    /// @brief Determines the result of the summary generation action by validating output files.
    /// @param [in] text_path The UTF-8 encoded path where the text summary was written (or empty if it was not generated).
    /// @param [in] json_path The UTF-8 encoded path where the JSON summary was written (or empty if it was not generated).
    /// @param [out] error The error string if the validation failed. This should be UTF-8 encoded.
    /// @return Result::kSuccess if the generated files are valid.
    static devtrace::Result DetermineResult(const std::string& text_path, const std::string& json_path, std::string& error);

    /// @brief Determines if a generated summary is valid at the given path.
    /// @param [in] path The path to check for a valid RGD summary.
    /// @return true if the summary was valid, false otherwise.
    static bool IsFileValid(const std::string& path);

    QSettings* tool_settings_;  ///< The settings used for the tool invoking this generator.
};
#endif
