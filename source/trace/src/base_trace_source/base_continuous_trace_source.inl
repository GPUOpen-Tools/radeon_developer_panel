// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Inline implmenetation for base continuous trace source.

// ReSharper disable once CppMissingIncludeGuard

namespace devtrace
{
    template <Continuous ClientType>
    BaseContinuousTraceSource<ClientType>::BaseContinuousTraceSource(
        const std::shared_ptr<typename BaseTraceSource<ClientType>::ClientFactoryType>& client_factory,
        const std::shared_ptr<ReadWriteStreamProvider>&                                 stream_provider,
        const std::shared_ptr<OverlayManager>&                                          overlay_manager,
        OverlayFeature                                                                  overlay_feature,
        const std::shared_ptr<Logger>&                                                  logger)
        : BaseTraceSource<ClientType>(client_factory, stream_provider, overlay_manager, overlay_feature, logger)
    {
    }

    template <Continuous ClientType>
    Result BaseContinuousTraceSource<ClientType>::RequestDump(uint16_t client_id)
    {
        const auto result = this->template WithClient<Result>(client_id, [](ClientType& client) { return client.RequestDump(); });
        return result.value_or(Result::kNotFound);
    }

    template <Continuous ClientType>
    Result BaseContinuousTraceSource<ClientType>::AddMarker(uint16_t client_id, const std::string& marker)
    {
        const auto result = this->template WithClient<Result>(client_id, [=](ClientType& client) { return client.AddMarker(marker); });
        return result.value_or(Result::kNotFound);
    }
}  // namespace devtrace
