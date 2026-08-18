// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for generic configurable object.

#ifndef RDP_SOURCE_TRACE_INC_CONFIGURABLE_H_
#define RDP_SOURCE_TRACE_INC_CONFIGURABLE_H_

#include "trace_source.h"

namespace devtrace
{
    /// @brief An object that has some kind of configuration.
    /// @tparam Config The type of the configuration object.
    template <typename Config>
    class Configurable
    {
    public:
        /// @brief Destructor.
        virtual ~Configurable() = default;

        /// @brief Gets the configuration for this object.
        /// @return The configuration object. Modifying this object will modify the configuration for this object.
        virtual Config& GetConfig() = 0;
    };

};  // namespace devtrace

#endif
