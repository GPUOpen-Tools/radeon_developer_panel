// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for an object that can generate RGD summaries.

#ifndef RDP_SOURCE_TRACE_INC_RGD_SUMMARY_GENERATOR_H_
#define RDP_SOURCE_TRACE_INC_RGD_SUMMARY_GENERATOR_H_

#include <atomic>
#include <string>

#include "dev_trace_common.h"
#include "rgd_summary_options.h"

namespace devtrace
{
    /// @brief An object that can generate RGD summaries.
    class RgdSummaryGenerator
    {
    public:
        /// @brief Destructor.
        virtual ~RgdSummaryGenerator() = default;

        /// @brief Generates either the text, json or both summaries for the dump at the path.
        ///
        /// This operation should be synchronous. The output of the summary should be put in the same folder as the dump.
        /// @param [in] dump_path_utf8 The path of the dump to generate the summary for. UTF-8 encoded.
        /// @param [in] generate_text true if the text summary should be generated, false otherwise.
        /// @param [in] generate_json true if the JSON summary should be generated, false otherwise.
        /// @param [out] error The error string if the generation failed. This should be UTF-8 encoded.
        /// @param [out] text_path The UTF-8 encoded path where the text summary was written (or empty of it was not generated).
        /// @param [out] json_path The UTF-8 encoded path where the JSON summary was written (or empty of it was not generated).
        /// @param [in] should_abort true if summary generation should be aborted.
        /// @param [in] options Additional options for the summary generation.
        /// @return Result::kSuccess if the generation was successful.
        virtual Result GenerateSummaries(const std::string&       dump_path_utf8,
                                         bool                     generate_text,
                                         bool                     generate_json,
                                         std::string&             error,
                                         std::string&             text_path,
                                         std::string&             json_path,
                                         std::atomic<bool>&       should_abort,
                                         const RgdSummaryOptions& options) = 0;
    };
}  // namespace devtrace

#endif
