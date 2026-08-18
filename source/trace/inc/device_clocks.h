// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the device clocks manager.

#ifndef RDP_SOURCE_TRACE_INC_DEVICE_CLOCKS_H_
#define RDP_SOURCE_TRACE_INC_DEVICE_CLOCKS_H_

#include <mutex>
#include <optional>
#include <unordered_set>
#include <vector>

#include <system_info_reader.h>

#include <dd_clocks_api.h>
#include <dd_connection_api.h>

#include <dipper.h>
#include <logging.h>

namespace devtrace
{
    /// @brief The type of clock mode
    enum class ClockModeType : uint8_t
    {
        kUnknown = 0,
        kNormal  = 1,
        kStable  = 2,
        kPeak    = 3
    };

    inline std::string GetClockModeTypeName(const ClockModeType type)
    {
        switch (type)
        {
        case ClockModeType::kNormal:
            return "Normal";
        case ClockModeType::kStable:
            return "Stable";
        case ClockModeType::kPeak:
            return "Peak";
        default:
            return "Unknown";
        }
    }

    /// @brief Details about a specific supported clock mode.
    struct ClockMode
    {
        std::string name;  ///< The name of the clock mode.
        std::string desc;  ///< The description of the clock mode.

        ClockModeType        type;     ///< The type of the clock mode.
        DD_DEVICE_CLOCK_MODE mode_id;  ///< The DevDriver id of the clock mode.

        uint64_t min_gpu_freq;  ///< GPU minimum shader clock frequency in hertz.
        uint64_t max_gpu_freq;  ///< GPU maximum shader clock frequency in hertz.

        uint64_t min_mem_freq;  ///< GPU minimum memory clock frequency in hertz.
        uint64_t max_mem_freq;  ///< GPU maximum memory clock frequency in hertz.
    };

    /// @brief Contains device clock information for a GPU.
    class DeviceClockGpu
    {
    public:
        /// @brief Constructor.
        /// @param [in] gpu_name The name of the GPU.
        /// @param gpu The info about the GPU.
        /// @param clocks_api The device clocks API to use.
        /// @param logger The logger to use.
        DeviceClockGpu(std::string gpu_name, const system_info_utils::GpuInfo& gpu, DDClocksApi* clocks_api, std::shared_ptr<Logger> logger);

        /// @brief Gets the name of the GPU.
        /// @return The name of the GPU.
        [[nodiscard]] const std::string& GetName() const;

        /// @brief Gets the ID of the GPU.
        /// @return The ID of the GPU.
        [[nodiscard]] uint32_t GetId() const;

    private:
        /// @brief Gets the clock mode type from the name.
        /// @param [in] name The name of the clock mode.
        /// @return The clock mode type.
        static ClockModeType GetClockModeType(const char* name);

        /// @brief Converts the DevDriver clocks mode to the API clock modes.
        /// @param [in] dd_modes The modes queried from DevDriver.
        void Convert(const std::vector<DDDeviceClocksClockModeInfo>& dd_modes);

    public:
        /// @brief Gets the current clock mode from the driver.
        /// @return The current clock mode for the GPU that was queried from the driver.
        [[nodiscard]] ClockModeType QueryClockMode() const;

        /// @brief Gets the clock modes for the GPU.
        /// @return The clock modes for the GPU.
        [[nodiscard]] const std::vector<ClockMode>& GetClockModes() const;

    protected:
        std::string                gpu_name_;              ///< The name of the GPU.
        system_info_utils::GpuInfo gpu_;                   ///< The GPU for this manager.
        DDClocksApi*               clocks_api_ = nullptr;  ///< The device clocks API to use.
        std::shared_ptr<Logger>    logger_;                ///< The logger to use.

        DDGpuId                gpu_id_ = 0;   ///< The id of the GPU.
        std::vector<ClockMode> clock_modes_;  ///< The clock modes for the GPU.
    };

    /// @brief Info about the device clocks GPU.
    struct DeviceClockGpuInfo
    {
        bool                                         is_connected = false;  ///< true if the info is from a connected system, false otherwise.
        std::vector<std::shared_ptr<DeviceClockGpu>> gpus;                  ///< The GPUs.
    };

    struct DeviceClockGpuEvent
    {
        void*                                                 listener;
        std::function<void(void*, const DeviceClockGpuInfo&)> callback;
    };

    /// @brief Device clock GPU that can be updated.
    class UpdatableDeviceClockGpu : public DeviceClockGpu
    {
    public:
        /// @brief Constructor.
        /// @param [in] gpu_name The name of the GPU.
        /// @param gpu The info about the GPU.
        /// @param clocks_api The device clocks API to use.
        /// @param logger The logger to use.
        UpdatableDeviceClockGpu(const std::string& gpu_name, const system_info_utils::GpuInfo& gpu, DDClocksApi* clocks_api, std::shared_ptr<Logger> logger);

        /// @brief Requests that the GPU use the given clock mode.
        /// @param [in] mode The clock mode to set for the GPU.
        /// @return true if the request was successful, false otherwise.
        [[nodiscard]] bool RequestMode(ClockModeType mode) const;

        /// @brief Sets whether this GPU should be forced to peak.
        /// @param [in] force_peak true if peak should be forced, false otherwise.
        /// @return true if setting to peak was successful, false otherwise.
        [[nodiscard]] bool SetForcePeak(bool force_peak);

    private:
        std::optional<ClockModeType> cached_mode_;  ///< The cached clock mode to restore
    };

    /// @brief Manager for device clocks.
    class DeviceClocksManager
    {
        static void OnDriverConnected(DDConnectionCallbacksImpl* manager_ptr, const DDConnectionInfo* connection_info);

        /// @brief Called when a router connects.
        /// @param [in] manager_ptr The manager to notify the router connected.
        /// @param [in] connection_id The router connection id.
        static void OnRouterConnected(DDConnectionCallbacksImpl* manager_ptr, DDConnectionId connection_id);

        /// @brief Called when a router disconnects.
        /// @param [in] manager_ptr The manager to notify the router disconnected.
        static void OnRouterDisconnected(DDConnectionCallbacksImpl* manager_ptr);

    public:
        /// @brief Constructor.
        /// @param [in] sys_info_cache  The system info cache.
        /// @param [in] overlay_manager The overlay manager.
        /// @param [in] connection_api The connection API.
        /// @param [in] clocks_api The device clocks API to use.
        /// @param [in] logger The logger to use.
        DIP(DeviceClocksManager(const std::shared_ptr<class SystemInfoCache>& sys_info_cache,
                                const std::shared_ptr<class OverlayManager>&  overlay_manager,
                                DDConnectionApi*                              connection_api,
                                DDClocksApi*                                  clocks_api,
                                std::shared_ptr<Logger>                       logger));

        /// @brief Destructor.
        ~DeviceClocksManager();

        /// @brief Unregisters this manager from connection callbacks.
        ///
        /// This should be called before the DDConnectionApi is destroyed.
        void Unregister();

        /// @brief Initializes the manager.
        bool Initialize();

    private:
        void OnDriverConnected() const;

        /// @brief Called when a router connects.
        void OnRouterConnected();

        /// @brief Called when a router disconnects.
        void OnRouterDisconnected();

    public:
        void RegisterEvent(const DeviceClockGpuEvent& event);

        /// @brief Gets the current clock mode from the driver.
        /// @param [in] gpu_id The id of the GPU to query the clock mode for.
        /// @return The current clock mode for the GPU that was queried from the driver.
        ClockModeType QueryClockMode(uint32_t gpu_id);

        /// @brief Requests that the GPU use the given clock mode.
        /// @param [in] gpu_id The id of the GPU to request.
        /// @param [in] mode The clock mode to set for the GPU.
        /// @return true if the request was successful, false otherwise.
        bool RequestMode(uint32_t gpu_id, ClockModeType mode);

        /// @brief Sets whether all the GPUs should be forced to use peak.
        /// @param [in] umd_connection_id The id of the connection that desires the clocks be set to peak.
        /// @param [in] force_peak true if peak should be forced, false otherwise.
        /// @return true if setting to peak was successful, false otherwise.
        bool SetForcePeak(DDConnectionId umd_connection_id, bool force_peak);

    private:
        /// @brief Updates the device clocks overlay.
        void UpdateOverlay() const;

        std::shared_ptr<SystemInfoCache> sys_info_cache_;   ///< The system info cache.
        std::shared_ptr<OverlayManager>  overlay_manager_;  ///< The overlay manager.

        DDConnectionApi*        connection_api_       = nullptr;  ///< The connection API.
        DDClocksApi*            clocks_api_           = nullptr;  ///< The device clocks API to use.
        bool                    callbacks_registered_ = false;    ///< Whether connection callbacks have been registered.
        std::shared_ptr<Logger> logger_;                          ///< The logger to use.

        std::recursive_mutex                                  gpus_mutex_;      ///< Mutex that guards the GPU array.
        DeviceClockGpuInfo                                    gpu_info_;        ///< List of connected GPU.
        std::vector<DeviceClockGpuEvent>                      events_;          ///< Registered event listeners and callbacks.
        std::vector<std::shared_ptr<UpdatableDeviceClockGpu>> updatable_gpus_;  ///< The editable GPUs managers.

        std::unordered_set<DDConnectionId> force_peak_umd_connection_ids_;  ///< The connections that desire clocks be at peak.
    };
}  // namespace devtrace

#endif
