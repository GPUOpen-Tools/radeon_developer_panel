// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for Developer Mode overlay manager.

#ifndef RDP_SOURCE_TRACE_OVERLAY_MANAGER_H_
#define RDP_SOURCE_TRACE_OVERLAY_MANAGER_H_

#include <condition_variable>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include <dd_common_api.h>

#include "dipper.h"

struct DDDriverUtilsApi;
struct DDConnectionApi;
struct DDConnectionCallbacksImpl;
struct DDConnectionInfo;
struct DDRouterUtilsApi;

namespace devtrace
{
    ///< The different features that are supported in the overlay.
    enum class OverlayFeature : uint8_t
    {
        kRgp = 0,
        kRmv,
        kRra,
        kRgd,
        kCount
    };

    ///< The state of each feature in the overlay.
    enum class OverlayState
    {
        kInactive = 0,
        kDisabled,
        kError,
        kDone,
        kBusy,
        kIdle,
        kWaitingToBeginCapture,
        kCapturing,
        kDumping,
        kProcessing
    };

    /// @brief Manager for the Developer Mode overlay.
    class OverlayManager
    {
        /// @brief Queue for updating the overlay for a client.
        ///
        /// We use this instead of FnWorkQueue since that queue needs to be empty before the destructor will return, but for the overlay
        /// we don't care about any queued items since the client is disconnecting. Additionally, this queue detaches the polling thread instead of joining it.
        struct OverlayQueue
        {
            /// @brief Constructor.
            OverlayQueue();

            /// @brief Destructor.
            ~OverlayQueue();

            /// @brief Queues an operation.
            /// @param [in] func The operation to queue.
            void Enqueue(const std::function<void()>& func) const;

            /// @brief Gets the thread for moving.
            /// @return The thread for moving.
            std::thread&& GetThread();

        private:
            std::thread thread_;  ///< The thread that the queue is polled on.

            /// @brief The state of the queue.
            struct State
            {
                std::atomic_bool                  poll_ = true;  ///< true if the queue should be polled.
                std::mutex                        mutex_;        ///< Lock that guards the queue.
                std::queue<std::function<void()>> queue_;        ///< The queue of operations.
                std::condition_variable           cnd_var_;      ///< The condition variable that
            };

            /// @brief The state of the queue. This queue thread also has a shared pointer so it can continue even after this object is destroyed.
            std::shared_ptr<State> state_;
        };

        /// @brief Stores the overlay state during client startup.
        using OverlayStartupState = std::unordered_map<OverlayFeature, OverlayState>;

        static constexpr uint8_t kDeviceClocksIndex = static_cast<uint8_t>(OverlayFeature::kCount);  ///< The line index that is used for device clocks.

        /// @brief Called when a router connects.
        /// @param [in] manager_ptr The manager to notify the router connected.
        /// @param [in] connection_id The router connection id.
        static void OnRouterConnected(DDConnectionCallbacksImpl* manager_ptr, DDConnectionId connection_id);

        /// @brief Called when a router disconnects.
        /// @param [in] manager_ptr The manager to notify the router disconnected.
        static void OnRouterDisconnected(DDConnectionCallbacksImpl* manager_ptr);

        /// @brief Called a new client connects.
        /// @param [in] manager_ptr The manager.
        /// @param [in] connection_info The information about the new connection.
        static void OnDriverConnected(DDConnectionCallbacksImpl* manager_ptr, const DDConnectionInfo* connection_info);

        /// @brief Called when the driver state of a connection changes.
        /// @param [in] manager_ptr The manager.
        /// @param [in] umd_connection_id The UMD connection id of the client.
        /// @param [in] state The current driver state of the client.
        static void OnDriverStateChanged(DDConnectionCallbacksImpl* manager_ptr, DDConnectionId umd_connection_id, DD_DRIVER_STATE state);

        /// @brief Called when a client disconnects.
        /// @param [in] manager_ptr The manager.
        /// @param [in] umd_connection_id The UMD connection id of the client.
        static void OnDriverDisconnected(DDConnectionCallbacksImpl* manager_ptr, DDConnectionId umd_connection_id);

    public:
        /// @brief Constructor.
        /// @param [in] driver_utils The API to use to update the overlay.
        /// @param [in] connection_api The connection API.
        /// @param [in] system_info_cache Cache for system info.
        DIP(OverlayManager(DDDriverUtilsApi* driver_utils, DDConnectionApi* connection_api, const std::shared_ptr<class SystemInfoCache>& system_info_cache));

        /// @brief Initializes the manager.
        bool Initialize();

        /// @brief Destructor.
        ~OverlayManager();

        /// @brief Updates the state visible in the overlay for a feature asynchronously.
        /// @param [in] umd_connection_id The connection to update the feature status for.
        /// @param [in] feature The feature to update.
        /// @param [in] state The state for the feature.
        void UpdateFeatureState(DDConnectionId umd_connection_id, OverlayFeature feature, OverlayState state);

        /// @brief Updates the device clocks info.
        /// @param [in] info Keys are GPU names, values are the names of the clock modes.
        void UpdateDeviceClocks(const std::map<std::string, std::string>& info);

    private:
        /// @brief Internal implementation of UpdateFeatureState() that calls the RPC using the queue for the client.
        /// @param [in] umd_connection_id The connection to update the feature status for.
        /// @param [in] feature The feature to update.
        /// @param [in] state The state for the feature.
        void UpdateFeatureStateInternal(DDConnectionId umd_connection_id, OverlayFeature feature, OverlayState state);

        /// @brief Called when a router connects.
        void OnRouterConnected();

        /// @brief Joins destroyed queue threads.
        void JoinQueueThreads();

        /// @brief Called a new client connects.
        /// @param [in] connection_info The information about the new connection.
        void OnDriverConnected(const DDConnectionInfo* connection_info);

        /// @brief Called when the driver state of a connection changes.
        /// @param [in] umd_connection_id The UMD connection id of the client.
        /// @param [in] state The current driver state of the client.
        void OnDriverStateChanged(DDConnectionId umd_connection_id, DD_DRIVER_STATE state);

        /// @brief Called when a client disconnects.
        /// @param [in] umd_connection_id The UMD connection id of the client.
        void OnDriverDisconnected(DDConnectionId umd_connection_id);

        /// @brief Updates the device clocks string for the given client.
        /// @param [in] umd_connection_id The client to update the device clock string for.
        void UpdateDeviceClocks(DDConnectionId umd_connection_id);

    public:
        /// @brief Unregisters this manager from connection callbacks.
        ///
        /// This should be called before the DDConnectionApi is destroyed.
        void Unregister();

    private:
        /// @brief Gets the current device clocks string.
        /// @return The current device clocks string.
        std::string GetDeviceClocksString() const;

        DDDriverUtilsApi*                driver_utils_         = nullptr;  ///< The API to use to update the overlay.
        DDConnectionApi*                 connection_api_       = nullptr;  ///< The connection API.
        bool                             callbacks_registered_ = false;    ///< Whether connection callbacks have been registered.
        std::shared_ptr<SystemInfoCache> system_info_cache_;               ///< Cache for system info.
        std::mutex                       state_mutex_;                     ///< Mutex that guards the state of the overlay manager.
        bool                             enabled_ = false;                 ///< true if the overlay manager is enabled, false otherwise.
        std::unordered_map<DDConnectionId, OverlayStartupState>
                                           startup_states_;     ///< The accumulated state of the overlay for a client before the client has finished starting.
        std::map<std::string, std::string> device_clock_info_;  ///< The map of GPUs and the name of their clock mode.
        std::unordered_map<DDConnectionId, std::unique_ptr<OverlayQueue>> queues_;        ///< The queue to use for RPC calls for each client.
        std::list<std::thread>                                            join_threads_;  ///< The threads from queues to join.
    };
}  // namespace devtrace

#endif
