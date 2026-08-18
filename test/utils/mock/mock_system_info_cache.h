// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for mock DevTrace system info cache.

#ifndef RDP_TEST_UTILS_MOCK_MOCK_SYSTEM_INFO_CACHE
#define RDP_TEST_UTILS_MOCK_MOCK_SYSTEM_INFO_CACHE

#include <system_info_cache.h>

class MockSystemInfoCache : public devtrace::SystemInfoCache
{
public:
    explicit MockSystemInfoCache(const DDModuleCommonApi& common_api)
        : SystemInfoCache(common_api)
    {
    }

    ~MockSystemInfoCache() override = default;

    MOCK_METHOD(DD_RESULT, GetSystemInfo, (DDModuleSystemContext, void*, PFN_ddReceiveText));
};

#endif
