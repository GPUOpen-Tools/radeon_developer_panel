// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for JSON test utils.

#ifndef RDP_TEST_UTILS_JSON_UTILS_H_
#define RDP_TEST_UTILS_JSON_UTILS_H_

#include <fstream>
#include <string>

#include "json/json_mapper.h"

class JsonUtils
{
public:
    /// @brief Loads the JSON at the specified path into a mapper for deserialization.
    /// @param [in] path The path to load the JSON from.
    /// @param [out] json_string A cache for the JSON file. If the string is not empty,
    ///                            the contents of the file are ignored and the contents of the string are used.
    ///                            After the file is loaded, the contents will be written to the cache.
    /// @return A JsonMapper for deserializing the loaded JSON data.
    static devtrace::JsonMapper LoadJsonFile(const char* path, std::string& json_string);

private:
    /// @brief Constructor.
    JsonUtils() = default;
};

#endif
