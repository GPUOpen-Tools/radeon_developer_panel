// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the system info cache.

#ifndef RDP_SOURCE_TRACE_INC_SYSTEM_INFO_CACHE_H_
#define RDP_SOURCE_TRACE_INC_SYSTEM_INFO_CACHE_H_

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

#include <tl/expected.hpp>

#include <ddModule.h>

#include "dipper.h"

struct DDRouterUtilsApi;

namespace devtrace
{
    /// @brief A class that will cache the system info for some short length of time.
    class SystemInfoCache final
    {
        /// @brief The clock used to generate timestamps.
        using Clock = std::chrono::high_resolution_clock;

        /// @brief The duration used for timestamps.
        using Duration = std::chrono::milliseconds;

        /// @brief The type used for the timestamps.
        using TimePoint = std::chrono::time_point<Clock, Duration>;

    public:
        /// @brief Constructor.
        /// @param router_utils_api The API used to query the system info.
        DIP(SystemInfoCache(DDRouterUtilsApi* router_utils_api));

        /// @brief Destructor.
        virtual ~SystemInfoCache() = default;

        /// @brief Retrieves the system info as a JSON string.
        /// If there is a valid, cached value for the system info, the cached value is returned. Otherwise, the system
        /// info will be queried from the router module.
        /// @return The system info JSON string on success, or an error code on failure.
        tl::expected<std::string, DD_RESULT> GetSystemInfoString();

        /// @brief Retrieves the system info.
        /// If there is a valid, cached value for the system info, the cached value is returned. Otherwise, the system
        /// info will be queried from the router module.
        /// @return The system info on success, or an error code on failure.
        tl::expected<system_info_utils::SystemInfo, DD_RESULT> GetSystemInfo();

    private:
        DDRouterUtilsApi* router_utils_api_ = nullptr;  ///< The API to use to get system info.

        std::mutex  cache_mutex_{};      ///< Mutex that guards read / write for the cache.
        std::string cached_info_ = "";   ///< The last cached system info JSON.
        TimePoint   last_cache_time_{};  ///< The last time that the system info was queried from the router.
    };
}  // namespace devtrace

#endif
