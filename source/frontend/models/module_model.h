// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the model that manages all the dev tools modules.

#ifndef RDP_SOURCE_FRONTEND_MODELS_MODULE_MODEL_H_
#define RDP_SOURCE_FRONTEND_MODELS_MODULE_MODEL_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <QAbstractItemModel>

#include <dev_trace_common.h>

#include <tl/expected.hpp>

struct DevToolsModule;
struct DDConnectionApi;
struct DDApiRegistry;
struct DDToolApi;
struct DDDriverUtilsApi;

class QSettings;

namespace devtrace
{
    class SystemInfoCache;
    class UbertraceUserFactory;
    class OverlayManager;
    class DeviceClocksManager;
}  // namespace devtrace

namespace rdp
{
    /// @brief Module that manages the DevTools modules.
    class ModuleModel final : public QAbstractItemModel
    {
        Q_OBJECT

        /// @brief Registers a module.
        /// @param [in] instance The module model to register it on.
        /// @param [in] module The module to register.
        static void RegisterModule(void* instance, DevToolsModule* module);

        /// @brief Unregisters a module.
        /// @param [in] instance The module model to unregister it on.
        /// @param [in] module The module to unregister.
        static void UnregisterModule(void* instance, const DevToolsModule* module);

        /// @brief Updates the serialized data for the module.
        /// @param [in] instance The module model to serialize the module on.
        /// @param [in] module The module to update the data for.
        /// @param [in] serialized_data The serialized data.
        static void ReceiveSerializeModule(void* instance, const DevToolsModule* module, const std::string& serialized_data);

        /// @brief Gets the settings for the tool.
        /// @param [in] instance The instance of the module API.
        /// @return The settings for the tool.
        static QSettings* GetToolSettings(void* instance);

        /// @brief Gets the UberTrace factory for the tool.
        /// @param [in] instance The instance of the module API.
        /// @return The UberTrace factory for the tool.
        static std::shared_ptr<devtrace::UbertraceUserFactory> GetUbertraceFactory(void* instance);

        /// @brief Gets the developer mode overlay manager for the tool.
        /// @param [in] instance The instance of the module API.
        /// @return The developer mode overlay manager for the tool.
        static std::shared_ptr<devtrace::OverlayManager> GetOverlayManager(void* instance);

        /// @brief Gets the device clocks manager.
        /// @param [in] instance The instance of the module API.
        /// @return The device clocks manager.
        static std::shared_ptr<devtrace::DeviceClocksManager> GetDeviceClocksManager(void* instance);

    public:
        /// @brief Constructor.
        /// @param [in] settings_manager The manager for the RDP settings.
        /// @param [in] parent The parent object of the model.
        explicit ModuleModel(const std::shared_ptr<class SettingsManager>& settings_manager, QObject* parent = nullptr);

        /// @brief Loads the model.
        /// @param [in] api_registry The API registry to use.
        /// @param [in] tool_api The API for the DD tool to use.
        [[nodiscard]] tl::expected<void, std::string> Load(DDApiRegistry* api_registry, const DDToolApi* tool_api);

    private:
        /// @brief Creates the overlay manager.
        /// @param [in] connection_api The DD connection API.
        /// @param [in] driver_utils_api The driver utils API.
        /// @param [in] sys_info_cache The system info cache.
        /// @return true if the operation was a success, false otherwise.
        bool CreateOverlayManager(DDConnectionApi*                                  connection_api,
                                  DDDriverUtilsApi*                                 driver_utils_api,
                                  const std::shared_ptr<devtrace::SystemInfoCache>& sys_info_cache);

        /// @brief Creates the device clocks manager.
        /// @param [in] api_registry The API registry to use.
        /// @param [in] connection_api The DD connection API.
        /// @param [in] sys_info_cache The system info cache.
        /// @return true if the operation was a success, false otherwise.
        bool CreateDeviceClocksManager(const DDApiRegistry*                              api_registry,
                                       DDConnectionApi*                                  connection_api,
                                       const std::shared_ptr<devtrace::SystemInfoCache>& sys_info_cache);

        /// @brief Registers APIs in the registry.
        /// @param [in] api_registry The API registry to use.
        /// @return true if the operation was a success, false otherwise.
        bool RegisterApis(const DDApiRegistry* api_registry);

        /// @brief Loads all the built-in modules.
        /// @param [in] api_registry The API registry to use.
        static void LoadBuiltinModules(DDApiRegistry* api_registry);

        /// @brief Registers a module.
        /// @param [in] module The module to register.
        void RegisterModule(DevToolsModule* module);

        /// @brief Unregisters a module.
        /// @param [in] module The module to unregister.
        void UnregisterModule(const DevToolsModule* module);

    public:
        /// @brief Called when the tool is about to be destroyed.
        void OnToolBeingDestroyed();

    signals:

        /// @brief Updates the serialized data for the module.
        /// @param [in] module The module to update the data for.
        /// @param [in] serialized_data The serialized data.
        void SerializeModule(const DevToolsModule* module, const std::string& serialized_data);

    private:
        /// @brief Gets the index of a module with the given name or -1 if it is not found.
        /// @param [in] name The name of the module to search for.
        /// @return The index of the module with the given name or -1 if it is not found.
        int GetModuleIndex(const std::string& name) const;

    public:
        /// @brief Gets all the currently loaded modules.
        /// @return All the currently loaded modules.
        std::vector<const DevToolsModule*> GetModules() const;

        /// @brief Gets all the currently enabled modules.
        /// @return All the currently enabled modules.
        std::vector<const DevToolsModule*> GetEnabledModules() const;

        /// @brief Returns true if any module needs per-process driver connections.
        ///
        /// This considers both explicitly enabled capture modules and system modules
        /// that report needing connections based on their current state.
        /// @return true if at least one module needs driver connections.
        bool HasModulesNeedingConnections() const;

        /// @brief Changes whether the module is enabled or disabled.
        /// @param [in] module The module to enable or disable.
        /// @param [in] is_enabled true if the module is enabled, false otherwise.
        void SetModuleEnabled(const DevToolsModule* module, bool is_enabled);

        /// @brief Changes whether the module is enabled or disabled.
        /// @param [in] module The name of the module to enable or disable.
        /// @param [in] is_enabled true if the module is enabled, false otherwise.
        void SetModuleEnabled(const std::string& module, bool is_enabled);

    signals:

        /// @brief Emitted when a module's enabled status changes.
        /// @param [in] module The module that had its status changed.
        /// @param [in] is_enabled true if the module is enabled, false otherwise.
        void ModuleStatusChanged(const DevToolsModule* module, bool is_enabled);

        /// @brief Emitted when trying to change a module's status fails.
        /// @param [in] module The module whose status failed to be changed.
        /// @param [in] is_enabled The desired state of the module - true if it should have been enabled, false if it should have been disabled.
        void ModuleFailedToChangeStatus(const DevToolsModule* module, bool is_enabled);

    public:
        /// @brief Locks modules from being enabled / disabled.
        void LockModules();

        /// @brief Allows modules to be enabled / disabled after being locked.
        void UnlockModules();

        /// @brief Returns whether modules are locked.
        /// @return true if modules are locked, false otherwise.
        bool AreModulesLocked() const;

    signals:
        /// @brief Emitted when modules are locked.
        void ModulesLocked();

        /// @brief Emitted when modules are unlocked.
        void ModulesUnlocked();

    public:
        /// @brief Resets the module to default.
        /// @param [in] module The module to default.
        void ResetModuleToDefault(const DevToolsModule* module) const;

        /// @brief Sets the data for a module.
        /// @param [in] module The module to set the data for.
        /// @param [in] data The serialized data for the module.
        void SetModuleData(const DevToolsModule* module, const std::string& data) const;

        /// @brief Returns true if the API is supported by one of the enabled modules.
        /// @param [in] api The API to check support for.
        /// @return true if the API is supported by one of the enabled modules, false otherwise.
        bool IsApiSupportedByEnabledModules(devtrace::Api api);

    private:
        /// @brief Returns true if the module is compatible with all the enabled modules.
        /// @return true if the module is compatible with existing modules, false otherwise.
        bool IsModuleCompatibleWithEnabledModules(const DevToolsModule* module) const;

        /// @brief Gets a list of modules that are incompatible with this module and are also enabled.
        /// @param [in] module The module to get the incompatible modules for.
        /// @return The list of modules that are not compatible with this provided module.
        std::vector<const DevToolsModule*> GetIncompatibleEnabledModules(const DevToolsModule* module) const;

    public:
        /// Sets the name of the application that is displayed in module views.
        /// @param [in] application_name The name of the application to display module views.
        void SetDisplayApplication(const QString& application_name) const;

        /// @brief The columns for the model
        enum ModuleModelColumns : uint8_t
        {
            kModuleModelColumnsDisplayName = 0,  ///< The column that has the display name
            kModuleModelColumnsEnabled,          ///< The column that has whether the module is enabled or not.
            kModuleModelColumnsIncompatible,     ///< The column that has whether the module is incompatible with other modules.
            kModuleModelColumnsCount             ///< The total number of columns.
        };

        // QAbstractItemModel

        /// @brief Gets the index at the specified row and column with the given parent.
        /// @param [in] row The row of the index.
        /// @param [in] column The column of the index.
        /// @param [in] parent The parent of the index.
        /// @return The index corresponding to the given row, column and parent.
        QModelIndex index(int row, int column, const QModelIndex& parent) const override;

        /// @brief Gets the parent index for the child.
        /// @param [in] child The child to get the parent index for.
        /// @return The parent index for the child.
        QModelIndex parent(const QModelIndex& child) const override;

        /// @brief Returns the number of rows under the given parent.
        /// @param [in] parent The parent to get the number of children for.
        /// @return The number of children for the parent.
        int rowCount(const QModelIndex& parent) const override;

        /// @brief Returns the number of columns under the given parent.
        /// @param [in] parent The parent to get the number of columns for.
        /// @return The number of columns for the parent.
        int columnCount(const QModelIndex& parent) const override;

        /// @brief Gets the data at the specified index for the given role.
        /// @param [in] index The index to get data for.
        /// @param [in] role The role to look at the data with.
        /// @return The data at the given index for the given role.
        QVariant data(const QModelIndex& index, int role) const override;

    private:
        /// @brief Gets the data at the specified index for Qt::DisplayRole.
        /// @param [in] module The module to get data for.
        /// @param [in] column The column to get data for.
        /// @return The data at the given index for Qt::DisplayRole.
        QVariant DisplayData(const DevToolsModule* module, int column) const;

        /// @brief Gets the data at the specified index for Qt::TooltipRole.
        /// @param [in] module The module to get data for.
        /// @param [in] column The column to get data for.
        /// @return The data at the given index for Qt::TooltipRole.
        QVariant TooltipData(const DevToolsModule* module, int column) const;

    public:
        /// @brief Gets the module at the given index.
        /// @param [in] index The index of the module to get.
        /// @return The module at the given index.
        const DevToolsModule* GetModuleAtIndex(const QModelIndex& index) const;

    private:
        std::shared_ptr<SettingsManager>                settings_manager_;       ///< The manager for the RDP settings.
        std::shared_ptr<devtrace::UbertraceUserFactory> ubertrace_factory_;      ///< The UberTrace user factory.
        std::shared_ptr<devtrace::OverlayManager>       overlay_manager_;        ///< Manager for developer mode overlay.
        std::shared_ptr<devtrace::DeviceClocksManager>  device_clocks_manager_;  ///< The device clocks manager.

        std::vector<DevToolsModule*> modules_;                 ///< All the modules that have been registered with the model.
        std::atomic<bool>            modules_locked_ = false;  ///< true if enabling / disabling modules is locked.
    };
}  // namespace rdp

#endif
