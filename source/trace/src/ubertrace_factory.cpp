// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for UberTrace factory.

#include "ubertrace_factory.h"

#include "base_trace_source/ubertrace_client.h"
#include "base_trace_source/ubertrace_features.h"

namespace devtrace
{
    UbertraceUserFactory::~UbertraceUserFactory()
    {
        delete features_;
    }

    std::unique_ptr<UbertraceUser> UbertraceUserFactory::GetUser(const ClientConnection& conn_info, DDUberTraceApi* ubertrace_api)
    {
        std::lock_guard lock(orchestrator_mutex_);
        if (!orchestrators_.contains(ubertrace_api))
        {
            orchestrators_.insert({ubertrace_api, {}});
        }

        auto&          api_orchestrators = orchestrators_[ubertrace_api];
        DDConnectionId umd_connection_id = conn_info.umd_connection_id;

        if (!api_orchestrators.contains(umd_connection_id))
        {
            UbertraceFeatures features{};
            {
                std::lock_guard feature_lock(features_mutex_);
                if (features_ != nullptr)
                {
                    features = *features_;
                }
            }

            auto orchestrator = std::make_shared<UbertraceOrchestrator>(conn_info, ubertrace_api, features);
            api_orchestrators.insert({umd_connection_id, orchestrator});
        }

        return api_orchestrators[umd_connection_id]->GetNewUser();
    }

    void UbertraceUserFactory::SetUbertraceFeatures(const UbertraceFeatures& features)
    {
        std::lock_guard lock(features_mutex_);
        if (features_ == nullptr)
        {
            features_ = new UbertraceFeatures();
        }

        *features_ = features;
    }
}  // namespace devtrace
