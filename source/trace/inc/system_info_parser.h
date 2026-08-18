// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for system info parser wrapper.

#ifndef RDP_SOURCE_TRACE_INC_SYSTEM_INFO_PARSER
#define RDP_SOURCE_TRACE_INC_SYSTEM_INFO_PARSER

#include "system_info_json_parser.h"

namespace devtrace
{
    /// @brief Wrapper around the static method from the RDP common module.
    class SystemInfoParser
    {
    public:
        /// @brief Parses the system information.
        /// @param [in] json_string  The JSON string to parse.
        /// @param [out] system_info The output for the parsed system information.
        /// @return true if the system information was successfully parsed, false otherwise.
        virtual bool Parse(const char* json_string, CompleteSystemInfo& system_info)
        {
            return SystemInfoJsonParser::ParseSystemInfo(json_string, system_info);
        }
    };
};  // namespace devtrace

#endif
