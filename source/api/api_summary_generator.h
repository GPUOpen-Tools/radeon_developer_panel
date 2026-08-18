// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for summary generator that doesn't do anything.

#ifndef SOURCE_API_CAPTURE_API_SUMMARY_GENERATOR_H_
#define SOURCE_API_CAPTURE_API_SUMMARY_GENERATOR_H_

#include <dipper.h>
#include <rgd_summary_generator.h>

/// @brief Summary generator that doesn't do anything.
class ApiSummaryGenerator : public devtrace::RgdSummaryGenerator
{
public:
    /// @brief Constructor.
    DIP(ApiSummaryGenerator()) = default;

    devtrace::Result GenerateSummaries([[maybe_unused]] const std::string&                 dump_path_utf8,
                                       [[maybe_unused]] bool                               generate_text,
                                       [[maybe_unused]] bool                               generate_json,
                                       [[maybe_unused]] std::string&                       error,
                                       [[maybe_unused]] std::string&                       text_path,
                                       [[maybe_unused]] std::string&                       json_path,
                                       [[maybe_unused]] std::atomic<bool>&                 should_abort,
                                       [[maybe_unused]] const devtrace::RgdSummaryOptions& options) override
    {
        return devtrace::Result::kFailure;
    }
};

#endif
