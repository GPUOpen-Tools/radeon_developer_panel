// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for UberTrace factory.

#ifndef RDP_SOURCE_TRACE_INC_UBERTRACE_FACTORY_H_
#define RDP_SOURCE_TRACE_INC_UBERTRACE_FACTORY_H_

#include <memory>
#include <mutex>
#include <unordered_map>

#include <dd_common_api.h>
#include <dd_uber_trace_api.h>

#include "client_connections.h"

#include "dipper.h"

namespace devtrace
{
    /// @brief Factory that creates UberTrace users.
    class UbertraceUserFactory
    {
    public:
        /// @brief Constructor.
        DIP(UbertraceUserFactory()) = default;

        /// @brief Destructor.
        ~UbertraceUserFactory();

        /// @brief Gets a new user for the connection.
        /// @param [in] conn_info The connection to get a user for.
        /// @param [in] ubertrace_api The API to use.
        /// @return A new user for the connection.
        std::unique_ptr<class UbertraceUser> GetUser(const ClientConnection& conn_info, DDUberTraceApi* ubertrace_api);

        /// @brief Sets the UberTrace features.
        /// @param [in] features The UberTrace driver features.
        void SetUbertraceFeatures(const struct UbertraceFeatures& features);

    private:
        /// @brief Type for orchestrator map under UberTrace API.
        using ApiOrchestrators = std::unordered_map<DDConnectionId, std::shared_ptr<class UbertraceOrchestrator>>;

        std::mutex                                            orchestrator_mutex_;  ///< Mutex to guard orchestrators.
        std::unordered_map<DDUberTraceApi*, ApiOrchestrators> orchestrators_{};     ///< Orchestrators.
        std::mutex                                            features_mutex_;      ///< Mutex that guards the features.
        UbertraceFeatures*                                    features_ = nullptr;  ///< The UberTrace driver features.
    };

}  // namespace devtrace

#endif
