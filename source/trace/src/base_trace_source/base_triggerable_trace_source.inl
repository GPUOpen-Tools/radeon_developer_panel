// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Inline implmenetation for base triggerable trace source.

// ReSharper disable once CppMissingIncludeGuard

namespace devtrace
{
    template <Triggerable ClientType>
    BaseTriggerableTraceSource<ClientType>::BaseTriggerableTraceSource(
        const std::shared_ptr<typename BaseTraceSource<ClientType>::ClientFactoryType>& client_factory,
        const std::shared_ptr<ReadWriteStreamProvider>&                                 stream_provider,
        const std::shared_ptr<OverlayManager>&                                          overlay_manager,
        OverlayFeature                                                                  overlay_feature,
        const std::shared_ptr<Logger>&                                                  logger,
        IClientEventBindingDelegate<ClientType>*                                        binding_delegate)
        : BaseTraceSource<ClientType>(client_factory, stream_provider, overlay_manager, overlay_feature, logger, binding_delegate)
    {
    }

    template <Triggerable ClientType>
    Result BaseTriggerableTraceSource<ClientType>::PrepareForDelayedCapture(const DDConnectionId umd_connection_id)
    {
        const auto result = this->template WithClient<Result>(umd_connection_id, [](ClientType& client) { return client.PrepareForDelayedCapture(); });
        return result.value_or(Result::kNotFound);
    }

    template <Triggerable ClientType>
    Result BaseTriggerableTraceSource<ClientType>::RequestBeginTrace(const DDConnectionId umd_connection_id, uint32_t capture_mode)
    {
        const auto result = this->template WithClient<Result>(umd_connection_id, [=](ClientType& client) { return client.RequestBeginTrace(capture_mode); });
        return result.value_or(Result::kNotFound);
    }

    template <Triggerable ClientType>
    void BaseTriggerableTraceSource<ClientType>::GetSupportedCaptureModes(const DDConnectionId         umd_connection_id,
                                                                          const std::vector<uint32_t>& candidates,
                                                                          std::vector<uint32_t>&       out_modes)
    {
        out_modes.clear();
        const auto result = this->template WithClient<bool>(umd_connection_id, [&](ClientType& client) {
            for (uint32_t candidate : candidates)
            {
                if (client.SupportsCaptureMode(candidate))
                {
                    out_modes.push_back(candidate);
                }
            }

            return true;
        });

        if (!result.has_value())
        {
            out_modes = {0};
            return;
        }

        DEV_TRACE_ASSERT(!out_modes.empty());
    }
}  // namespace devtrace
