// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the system info cache.

#include "system_info_cache.h"

#include <dd_router_utils_api.h>

#include "dev_trace_common.h"

namespace devtrace
{
    /// @brief The number of increments of Duration where the cached system info value is still valid.
    static constexpr uint32_t kCacheDurationIncrements = 100;

    SystemInfoCache::SystemInfoCache(DDRouterUtilsApi* router_utils_api)
        : router_utils_api_(router_utils_api)
    {
    }

    tl::expected<std::string, DD_RESULT> SystemInfoCache::GetSystemInfoString()
    {
        const std::scoped_lock cache_lock(cache_mutex_);
        if (router_utils_api_ == nullptr)
        {
            return tl::unexpected(DD_RESULT_UNKNOWN);
        }

        const TimePoint now = std::chrono::time_point_cast<Duration>(Clock::now());

        if (const auto elapsed_time = now - last_cache_time_; elapsed_time <= Duration(kCacheDurationIncrements))
        {
            return cached_info_;
        }

        size_t      required_size;
        std::string json_buffer;

        DD_RESULT result = router_utils_api_->GetSysInfo(router_utils_api_->pInstance, nullptr, &required_size);
        if (result != DD_RESULT_SUCCESS)
        {
            return tl::unexpected(result);
        }

        json_buffer.resize(required_size);
        result = router_utils_api_->GetSysInfo(router_utils_api_->pInstance, json_buffer.data(), &required_size);
        if (result != DD_RESULT_SUCCESS)
        {
            return tl::unexpected(result);
        }

        cached_info_     = json_buffer;
        last_cache_time_ = std::chrono::time_point_cast<Duration>(Clock::now());
        return cached_info_;
    }

    tl::expected<system_info_utils::SystemInfo, DD_RESULT> SystemInfoCache::GetSystemInfo()
    {
        const std::scoped_lock cache_lock(cache_mutex_);
        if (router_utils_api_ == nullptr)
        {
            return tl::unexpected(DD_RESULT_UNKNOWN);
        }

        const TimePoint now = std::chrono::time_point_cast<Duration>(Clock::now());

        if (const auto elapsed_time = now - last_cache_time_; elapsed_time <= Duration(kCacheDurationIncrements))
        {
            if (system_info_utils::SystemInfo info; system_info_utils::SystemInfoReader::Parse(cached_info_, info))
            {
                return info;
            }
            return tl::unexpected(DD_RESULT_UNKNOWN);
        }

        size_t      required_size;
        std::string json_buffer;

        DD_RESULT result = router_utils_api_->GetSysInfo(router_utils_api_->pInstance, nullptr, &required_size);
        if (result != DD_RESULT_SUCCESS)
        {
            return tl::unexpected(result);
        }

        json_buffer.resize(required_size);
        result = router_utils_api_->GetSysInfo(router_utils_api_->pInstance, json_buffer.data(), &required_size);
        if (result != DD_RESULT_SUCCESS)
        {
            return tl::unexpected(result);
        }

        cached_info_     = json_buffer;
        last_cache_time_ = std::chrono::time_point_cast<Duration>(Clock::now());
        if (system_info_utils::SystemInfo info; system_info_utils::SystemInfoReader::Parse(cached_info_, info))
        {
            return info;
        }

        return tl::unexpected(DD_RESULT_UNKNOWN);
    }
}  // namespace devtrace
