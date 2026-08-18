// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for Developer Mode overlay manager.

#include "overlay_manager.h"

#include <chrono>
#include <sstream>
#include <string>

#include <dd_connection_api.h>
#include <dd_driver_utils_api.h>

#include <fmt/format.h>

#include <ranges>

#include "source_status.h"
#include "system_info_cache.h"

static constexpr int kMinimumCustomOverlayDriverMajorVersion = 24;
static constexpr int kMinimumCustomOverlayDriverMinorVersion = 10;

namespace devtrace
{
    OverlayManager::OverlayQueue::OverlayQueue()
    {
        state_ = std::make_shared<State>();

        thread_ = std::thread([state = state_] {
            while (state->poll_)
            {
                std::unique_lock lock(state->mutex_);
                if (state->queue_.empty())
                {
                    if (!state->cnd_var_.wait_for(lock, std::chrono::milliseconds(100), [&] { return !state->queue_.empty(); }))
                    {
                        continue;
                    }
                }

                if (!state->poll_)
                {
                    return;
                }

                std::function<void()> func = std::move(state->queue_.front());
                state->queue_.pop();

                lock.unlock();
                if (func)
                {
                    func();
                }
            }
        });
    }

    OverlayManager::OverlayQueue::~OverlayQueue()
    {
        state_->poll_ = false;

        DEV_TRACE_ASSERT(!thread_.joinable());
    }

    void OverlayManager::OverlayQueue::Enqueue(const std::function<void()>& func) const
    {
        std::lock_guard lock(state_->mutex_);
        state_->queue_.push(func);

        state_->cnd_var_.notify_one();
    }

    std::thread&& OverlayManager::OverlayQueue::GetThread()
    {
        return std::move(thread_);
    }

    void OverlayManager::OnRouterConnected(DDConnectionCallbacksImpl* manager_ptr, [[maybe_unused]] DDConnectionId connection_id)
    {
        auto* manager = reinterpret_cast<OverlayManager*>(manager_ptr);
        manager->OnRouterConnected();
    }

    void OverlayManager::OnRouterDisconnected(DDConnectionCallbacksImpl* manager_ptr)
    {
        auto* manager = reinterpret_cast<OverlayManager*>(manager_ptr);
        manager->JoinQueueThreads();
    }

    void OverlayManager::OnDriverConnected(DDConnectionCallbacksImpl* manager_ptr, const DDConnectionInfo* connection_info)
    {
        auto* manager = reinterpret_cast<OverlayManager*>(manager_ptr);
        manager->OnDriverConnected(connection_info);
    }

    void OverlayManager::OnDriverStateChanged(DDConnectionCallbacksImpl* manager_ptr, const DDConnectionId umd_connection_id, const DD_DRIVER_STATE state)
    {
        auto* manager = reinterpret_cast<OverlayManager*>(manager_ptr);
        manager->OnDriverStateChanged(umd_connection_id, state);
    }

    void OverlayManager::OnDriverDisconnected(DDConnectionCallbacksImpl* manager_ptr, const DDConnectionId umd_connection_id)
    {
        auto* manager = reinterpret_cast<OverlayManager*>(manager_ptr);
        manager->OnDriverDisconnected(umd_connection_id);
    }

    OverlayManager::OverlayManager(DDDriverUtilsApi* driver_utils, DDConnectionApi* connection_api, const std::shared_ptr<SystemInfoCache>& system_info_cache)
        : driver_utils_(driver_utils)
        , connection_api_(connection_api)
        , system_info_cache_(system_info_cache)
    {
    }

    bool OverlayManager::Initialize()
    {
        Unregister();

        DDConnectionCallbacks connection_callbacks{};
        connection_callbacks.pImpl                = reinterpret_cast<DDConnectionCallbacksImpl*>(this);
        connection_callbacks.OnDriverConnected    = &OverlayManager::OnDriverConnected;
        connection_callbacks.OnDriverDisconnected = &OverlayManager::OnDriverDisconnected;
        connection_callbacks.OnDriverStateChanged = &OverlayManager::OnDriverStateChanged;
        connection_callbacks.OnRouterConnected    = &OverlayManager::OnRouterConnected;
        connection_callbacks.OnRouterDisconnected = &OverlayManager::OnRouterDisconnected;

        callbacks_registered_ = connection_api_->AddConnectionCallbacks(connection_api_->pInstance, &connection_callbacks) == DD_RESULT_SUCCESS;
        return callbacks_registered_;
    }

    OverlayManager::~OverlayManager()
    {
        Unregister();
    }

    // ReSharper disable once CppDFAUnreachableFunctionCall
    void OverlayManager::OnRouterConnected()
    {
        std::lock_guard lock(state_mutex_);
        if (system_info_cache_ == nullptr)
        {
            enabled_ = false;
            return;
        }

        auto e_system_info = system_info_cache_->GetSystemInfo();
        if (!e_system_info.has_value())
        {
            enabled_ = false;
            return;
        }

        const system_info_utils::SystemInfo sys_info = std::move(*e_system_info);
        enabled_ = !IsDriverTooOld(sys_info.driver, kMinimumCustomOverlayDriverMajorVersion, kMinimumCustomOverlayDriverMinorVersion);
    }

    void OverlayManager::JoinQueueThreads()
    {
        std::list<std::thread> join_threads;
        {
            std::lock_guard lock(state_mutex_);
            join_threads = std::move(join_threads_);
        }

        for (std::thread& thread : join_threads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
    }

    // ReSharper disable once CppDFAUnreachableFunctionCall
    void OverlayManager::OnDriverConnected(const DDConnectionInfo* connection_info)
    {
        // We join any queue threads here so that the list of threads to join does not slowly accumulate as more and more connections exit.
        // This will make client startup slower, but shouldn't interfere with RMV since the disconnect isn't blocked by a join.
        JoinQueueThreads();

        std::lock_guard lock(state_mutex_);
        if (!enabled_)
        {
            return;
        }

        const DDConnectionId umd_connection_id = connection_info->umdConnectionId;
        queues_.insert({umd_connection_id, std::make_unique<OverlayQueue>()});
        startup_states_.insert({umd_connection_id, {}});

        auto& state = startup_states_[umd_connection_id];
        for (uint8_t feature = 0; feature < static_cast<uint8_t>(OverlayFeature::kCount); ++feature)
        {
            state.insert({static_cast<OverlayFeature>(feature), OverlayState::kInactive});
        }
    }

    // ReSharper disable once CppDFAUnreachableFunctionCall
    void OverlayManager::OnDriverStateChanged(const DDConnectionId umd_connection_id, const DD_DRIVER_STATE state)
    {
        if (state != DD_DRIVER_STATE_DEVICEINIT)
        {
            return;
        }

        std::lock_guard lock(state_mutex_);
        if (!enabled_ || !startup_states_.contains(umd_connection_id))
        {
            return;
        }

        const OverlayStartupState startup_state = std::move(startup_states_[umd_connection_id]);
        startup_states_.erase(umd_connection_id);

        for (const auto& [feature, state] : startup_state)
        {
            UpdateFeatureStateInternal(umd_connection_id, feature, state);
        }

        UpdateDeviceClocks(umd_connection_id);
    }

    // ReSharper disable once CppDFAUnreachableFunctionCall
    void OverlayManager::OnDriverDisconnected(const DDConnectionId umd_connection_id)
    {
        std::lock_guard lock(state_mutex_);
        startup_states_.erase(umd_connection_id);

        if (!queues_.contains(umd_connection_id))
        {
            return;
        }

        join_threads_.emplace_back(queues_[umd_connection_id]->GetThread());
        queues_.erase(umd_connection_id);
    }

    void OverlayManager::Unregister()
    {
        if (connection_api_ != nullptr && callbacks_registered_)
        {
            connection_api_->RemoveConnectionCallbacks(connection_api_->pInstance, reinterpret_cast<DDConnectionCallbacksImpl*>(this));
            callbacks_registered_ = false;
        }
    }

    void OverlayManager::UpdateFeatureState(const DDConnectionId umd_connection_id, OverlayFeature feature, OverlayState state)
    {
        std::lock_guard lock(state_mutex_);
        if (!enabled_)
        {
            return;
        }

        if (!startup_states_.contains(umd_connection_id))
        {
            UpdateFeatureStateInternal(umd_connection_id, feature, state);
            return;
        }

        if (auto& startup_state = startup_states_[umd_connection_id]; !startup_state.contains(feature))
        {
            startup_state.insert({feature, state});
        }
        else
        {
            startup_state[feature] = state;
        }
    }

    inline std::string GetNameForFeature(const OverlayFeature feature)
    {
        switch (feature)
        {
        case OverlayFeature::kRgp:
            return "RGP Profiling";
        case OverlayFeature::kRmv:
            return "RMV Tracing";
        case OverlayFeature::kRra:
            return "RRA Capture";
        case OverlayFeature::kRgd:
            return "Crash Analysis";
        default:
            return "Unknown Feature";
        }
    }

    inline std::string GetStringForOverlayState(const OverlayState state)
    {
        switch (state)
        {
        case OverlayState::kInactive:
            return "Inactive";
        case OverlayState::kDisabled:
            return "Unsupported";
        case OverlayState::kError:
            return "Error";
        case OverlayState::kDone:
            return "Done";
        case OverlayState::kBusy:
            return "Busy";
        case OverlayState::kIdle:
            return "Ready";
        case OverlayState::kWaitingToBeginCapture:
            return "Waiting to begin capture";
        case OverlayState::kCapturing:
            return "Capturing";
        case OverlayState::kDumping:
            return "Dumping";
        case OverlayState::kProcessing:
            return "Processing";
        default:
            return "Unknown";
        }
    }

    void OverlayManager::UpdateFeatureStateInternal(const DDConnectionId umd_connection_id, OverlayFeature feature, const OverlayState state)
    {
        if (!queues_.contains(umd_connection_id) || driver_utils_ == nullptr)
        {
            return;
        }

        queues_[umd_connection_id]->Enqueue([=, this] {
            const std::string overlay_string = fmt::format("{}: {}", GetNameForFeature(feature), GetStringForOverlayState(state));
            driver_utils_->SetDriverOverlayString(driver_utils_->pInstance, umd_connection_id, overlay_string.c_str(), static_cast<uint32_t>(feature));
        });
    }

    void OverlayManager::UpdateDeviceClocks(const std::map<std::string, std::string>& info)
    {
        std::lock_guard lock(state_mutex_);
        if (!enabled_)
        {
            return;
        }

        device_clock_info_ = info;

        for (const auto& umd_connection_id : queues_ | std::views::keys)
        {
            UpdateDeviceClocks(umd_connection_id);
        }
    }

    void OverlayManager::UpdateDeviceClocks(const DDConnectionId umd_connection_id)
    {
        // If the client hasn't finished starting up, there's no point in queuing an update since it will be done at the end of startup.
        if (startup_states_.contains(umd_connection_id) || !queues_.contains(umd_connection_id) || driver_utils_ == nullptr)
        {
            return;
        }

        queues_[umd_connection_id]->Enqueue([=, this] {
            std::string clock_string;
            {
                std::lock_guard lock(state_mutex_);
                clock_string = GetDeviceClocksString();
            }

            if (clock_string.empty())
            {
                return;
            }

            driver_utils_->SetDriverOverlayString(driver_utils_->pInstance, umd_connection_id, clock_string.c_str(), kDeviceClocksIndex);
        });
    }

    std::string OverlayManager::GetDeviceClocksString() const
    {
        if (device_clock_info_.empty())
        {
            return "";
        }

        std::ostringstream stream;
        stream << "Device Clocks: ";

        size_t added_gpus = 0;
        for (const auto& [gpu, clock_mode] : device_clock_info_)
        {
            stream << gpu << " - " << clock_mode;
            ++added_gpus;

            if (added_gpus != device_clock_info_.size())
            {
                stream << ", ";
            }
        }

        return stream.str();
    }
}  // namespace devtrace
