// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for simple dependency injection.

#include "dipper.h"

namespace dipper
{

    Resolver::Resolver(const std::shared_ptr<Factories>& factories)
        : factories_(factories)

    {
    }

    Container::Container()
        : resolver_(std::make_shared<Resolver>(factories_))
    {
    }

    std::shared_ptr<Resolver> Container::GetResolver() const
    {
        return resolver_;
    }

    void Container::Install(const Container& other)
    {
        for (const auto& pair : *other.factories_)
        {
            DEV_TRACE_ASSERT_MSG(factories_->count(pair.first) == 0, "An existing factory already existed");
            factories_->insert({pair.first, pair.second->Copy()});
        }
    }

}  // namespace dipper
