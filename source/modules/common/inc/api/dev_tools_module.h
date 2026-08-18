// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  The definition for the shared DevTools module API and interfaces.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_API_DEV_TOOLS_MODULE_H_
#define RDP_SOURCE_MODULES_COMMON_INC_API_DEV_TOOLS_MODULE_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include <QWidget>

#include <ddApi.h>

#include <dd_connection_api.h>
#include <dev_trace_common.h>

static constexpr const char* kDevToolsModuleApiName         = "DevToolsModuleApi";  ///< The name of the DevTools module api.
static constexpr uint32_t    kDevToolsModuleApiVersionMajor = 1;                    ///< The major version of the DevTools module api.
static constexpr uint32_t    kDevToolsModuleApiVersionMinor = 0;                    ///< The minor version of the DevTools module api.
static constexpr uint32_t    kDevToolsModuleApiVersionPatch = 0;                    ///< The patch of the DevTools module api.

static constexpr const char* kDevTraceDependenciesApiName  = "DevTraceDependenciesApi";  ///< The name of the devtrace dependencies api.
static constexpr uint32_t    kDevTraceDependenciesApiMajor = 1;                          ///< The major version of the devtrace dependencies api.
static constexpr uint32_t    kDevTraceDependenciesApiMinor = 0;                          ///< The minor version of the devtrace dependencies api.
static constexpr uint32_t    kDevTraceDependenciesApiPatch = 0;                          ///< The patch of the devtrace dependencies api.

namespace devtrace
{
    class ClientConnectionManager;
    class ClientConnectionManagerV2;
    class TraceSource;
    class UbertraceUserFactory;
    class OverlayManager;
    class DeviceClocksManager;

}  // namespace devtrace

/// @brief The API for DevTools module registration.
struct DevToolsModuleApi
{
    /// @brief The instance of the module API.
    void* instance = nullptr;

    /// @brief Registers a module.
    /// @param [in] instance The instance of the module API to register the module with.
    /// @param [in] module The module to register.
    void (*register_module)(void* instance, struct DevToolsModule* module) = nullptr;

    /// @brief Unregisters a module.
    /// @param [in] instance The instance of the module API to unregister the module with.
    /// @param [in] module The module to unregister.
    void (*unregister_module)(void* instance, const struct DevToolsModule* module) = nullptr;

    /// @brief Updates the serialized data for the module.
    /// @param [in] instance The instance of the module API.
    /// @param [in] module The module to update the data for.
    /// @param [in] serialized_data The serialized data.
    void (*serialize_module)(void* instance, const struct DevToolsModule* module, const std::string& serialized_data) = nullptr;

    /// @brief Gets the settings for the tool.
    /// @param [in] instance The instance of the module API.
    /// @return The settings for the tool.
    class QSettings* (*get_tool_settings)(void* instance) = nullptr;
};

/// @brief API for getting tool-level devtrace APIs.
struct DevTraceDependenciesApi
{
    /// @brief The instance of the module API.
    void* instance = nullptr;

    /// @brief Gets the UberTrace user factory for the tool.
    /// @param [in] instance The instance of the module API.
    /// @return The UberTrace factory.
    std::shared_ptr<devtrace::UbertraceUserFactory> (*get_ubertrace_factory)(void* instance) = nullptr;

    /// @brief Gets the developer mode overlay manager for the tool.
    /// @param [in] instance The instance of the module API.
    /// @return The developer mode overlay manager.
    std::shared_ptr<devtrace::OverlayManager> (*get_overlay_manager)(void* instance) = nullptr;

    /// @brief Gets the device clocks manager.
    /// @param [in] instance The instance of the module API.
    /// @return The device clocks manager.
    std::shared_ptr<devtrace::DeviceClocksManager> (*get_device_clocks_manager)(void* instance) = nullptr;
};

/// @brief Categories for DevTools modules.
///
/// Capture modules are hosted in the CAPTURE view and are tied to application connections.
/// System modules are hosted in the SYSTEM view and operate system-wide, independent of connections.
enum class ModuleCategory : uint32_t
{
    kCapture = 0,
    kSystem  = 1
};

/// @brief The interface for a DevTools module.
///
/// There is some implementation here to reduce boiler plate in each module that implements the interface.
struct DevToolsModule
{
private:
    /// @brief Calls InitializeAndRegister on the instance.
    /// @param [in] module_instance_ptr The instance to call InitializeAndRegister() on.
    /// @return the result of the initialization.
    static DD_RESULT InitializeModule(struct DDModuleInstance* module_instance_ptr);

    /// @brief Calls UnregisterAndDestroy on the instance.
    /// @param [in] module_instance_ptr The instance to call UnregisterAndDestroy() on.
    static void DestroyModule(struct DDModuleInstance* module_instance_ptr);

    /// @brief Called when a DDTool connects to a router.
    /// @param [in] instance The connection model that registered the callback.
    /// @param [in] connection_id The id that uniquely represents the connection to DDRouter.
    static void OnRouterConnected(struct DDConnectionCallbacksImpl* instance, DDConnectionId connection_id);

protected:
    /// @brief Called when a DDTool connects to a router.
    virtual void HandleOnRouterConnected();

private:
    /// @brief Called when a DDTool disconnects from a router.
    /// @param [in] instance The connection model that registered the callback.
    static void OnRouterDisconnected(struct DDConnectionCallbacksImpl* instance);

protected:
    /// @brief Called when a DDTool disconnects from a router.
    virtual void HandleOnRouterDisconnected();

public:
    /// @brief Default constructor.
    /// @param [in] api_registry The API registry to use for this module
    /// @param [in] name The name of the module.
    /// @param [in] display_name The display name of the module.
    explicit DevToolsModule(struct DDApiRegistry* api_registry, const std::string& name, const std::string& display_name);

    /// @brief Destructor.
    virtual ~DevToolsModule();

private:
    /// @brief Initializes the model and then registers it with the DevTools module API.
    /// @return the result of the initialization.
    DD_RESULT InitializeAndRegister();

    /// @brief Loads APIs from the API registry that are needed for the module.
    /// @return the result of the loading.
    DD_RESULT LoadApis();

    /// @brief Registers the router connection callbacks.
    DD_RESULT RegisterConnectionCallbacks();

    /// @brief
    void UnregisterAndDestroy();

protected:
    /// @brief Initializes the module.
    /// @param [in] tool_settings The tool settings.
    /// @return the result of the initialization.
    virtual DD_RESULT Initialize([[maybe_unused]] QSettings* tool_settings)
    {
        return DD_RESULT_SUCCESS;
    }

    /// @brief Performs any cleanup that the module needs.
    virtual void Destroy()
    {
    }

public:
    /// @brief Gets the name of the module.
    /// @return The name of the module.
    [[nodiscard]] const std::string& GetModuleName() const;

    /// @brief Gets the name of the module used for display.
    /// @return The display name of the module.
    [[nodiscard]] const std::string& GetModuleDisplayName() const;

    /// @brief Gets the category of the module.
    /// @return The module category (kCapture or kSystem).
    [[nodiscard]] virtual ModuleCategory GetModuleCategory() const;

    /// @brief Resets all the settings in the module to default.
    virtual void ResetToDefaults();

    /// @brief Serializes the userdata using the module API.
    /// @param [in] serialized_data The serialized data.
    virtual void SerializeData(const std::string& serialized_data);

    /// @brief Loads serialized data.
    /// @param [in] data The serialized data for the model.
    /// @return the driver settings error type.
    [[nodiscard]] virtual bool LoadSerializedData(const std::string& data);

    /// @brief Returns whether the module is compatible with the given API.
    /// @param [in] api The API to check module compatibility against.
    /// @return true if the module is compatible with the API, false otherwise.
    [[nodiscard]] virtual bool IsCompatibleWithApi(devtrace::Api api) const;

    /// @brief Returns whether the module is compatible with another module with the given name.
    ///
    /// Module names are unique, but a allow-list is recommended if the module is not compatible with others, since this will prevent unintended interractions.
    /// @param [in] module_name The API to check module compatibility against.
    /// @return true if the module is compatible with the module, false otherwise.
    [[nodiscard]] virtual bool IsCompatibleWithModuleNamed(const std::string& module_name) const;

    /// @brief Sets whether the module is enabled.
    /// @param [in] is_enabled true if the module should be enabled, false otherwise.
    /// @return true if changing the enabled status of the module was successful, false otherwise.
    bool SetEnabled(bool is_enabled);

    /// @brief Returns true if the module is enabled, false otherwise.
    /// @return true if the module is enabled, false otherwise.
    [[nodiscard]] bool IsEnabled() const;

    /// @brief Returns true if this module requires per-process driver connections.
    ///
    /// The default implementation returns IsEnabled(). System modules may override
    /// this to indicate their connection needs based on module-specific state.
    /// @return true if the module needs driver connections, false otherwise.
    [[nodiscard]] virtual bool NeedsDriverConnections() const;

protected:
    /// @brief Enables the module.
    /// @return true if the module was enabled, false otherwise.
    virtual bool Enable();

    /// @brief Disables the module.
    /// @return true if the module was disabled, false otherwise.
    virtual bool Disable();

public:
    /// @brief Creates a module's view.
    ///
    /// Memory for the model will be managed by the caller, and this should be able to be called multiple times without issues.
    /// @return A new view for this module.
    [[nodiscard]] virtual QWidget* CreateView() const = 0;

    /// Sets the name of the application that is displayed in views for this module.
    /// @param [in] application_name The name of the application to display in views for this module.
    virtual void SetDisplayApplication(const QString& application_name);

private:
    DevToolsModuleApi*       dev_tools_module_api_ = nullptr;  /// The API to use to register and unregister the module.
    DevTraceDependenciesApi* devtrace_deps_api_    = nullptr;  ///< The API to get tool level devtrace deps.

protected:
    DDApiRegistry* api_registry_    = nullptr;  ///< The API registry to use for this model.
    bool           has_initialized_ = false;    ///< true if the module has been initialized, false otherwise.

    std::mutex        enabled_mutex_;       ///< Mutex that guards enabling / disabling.
    std::atomic<bool> is_enabled_ = false;  ///< true if the module is enabled, false otherwise.

    std::shared_ptr<class UserdataViewModel>    base_userdata_view_model_;      ///< The base userdata view model.
    std::shared_ptr<class TraceSourceViewModel> base_trace_source_view_model_;  ///< The base trace source view model.

    std::shared_ptr<devtrace::TraceSource>               base_trace_source_;      ///< The base trace source.
    std::unique_ptr<devtrace::ClientConnectionManagerV2> connection_manager_v2_;  ///< The object that manages connections to the driver.

    std::shared_ptr<devtrace::UbertraceUserFactory> ubertrace_factory_;      ///< The UberTrace client coordinator.
    std::shared_ptr<devtrace::OverlayManager>       overlay_manager_;        ///< Manager for the developer mode overlay.
    std::shared_ptr<devtrace::DeviceClocksManager>  device_clocks_manager_;  ///< The device clocks manager.

    std::string name_;          ///< The name of the module.
    std::string display_name_;  ///< The display name of the module.

    struct DDModulesCallbacks* module_callbacks_ = nullptr;  ///< The module callbacks for this module.
    struct DDRouterUtilsApi*   router_utils_api_ = nullptr;  ///< The API to use to get system info.
    struct DDDriverUtilsApi*   driver_utils_api_ = nullptr;  ///< The API used for driver utils.
    DDConnectionApi*           connection_api_   = nullptr;  ///< The API used for connections.
    struct DevToolsLoggingApi* logging_api_      = nullptr;  ///< The API used for logging.
};

#endif
