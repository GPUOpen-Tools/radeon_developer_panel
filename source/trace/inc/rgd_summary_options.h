// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for RGD summary options.

#ifndef RDP_SOURCE_TRACE_INC_RGD_SUMMARY_OPTIONS_H_
#define RDP_SOURCE_TRACE_INC_RGD_SUMMARY_OPTIONS_H_

#include <string>
#include <vector>

namespace devtrace
{
    /// @brief The additional options for RGD summary generation.
    struct RgdSummaryOptions
    {
        bool show_marker_source = false;  ///< true if summaries should have source information for markers.
        bool expand_markers     = false;  ///< true if summaries should expand the marker tree.

        std::vector<std::string> pdb_search_paths{};              ///< The PDB search paths.
        bool                     pdb_include_subfolders = false;  ///< true if PDB search should include subfolders.
    };

}  // namespace devtrace

#endif
