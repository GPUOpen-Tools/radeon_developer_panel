// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for JSON test utils.

#include "json_utils.h"

devtrace::JsonMapper JsonUtils::LoadJsonFile(const char* path, std::string& json_string)
{
    if (json_string.empty())
    {
        std::ifstream file_stream(path);
        if (!file_stream.bad())
        {
            std::string line;
            while (std::getline(file_stream, line))
            {
                json_string += line;
            }
        }

        file_stream.close();
    }

    return devtrace::JsonMapper::Deserialize(json_string);
}
