// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Managed Apps Model class definition

#ifndef RDP_SOURCE_FRONTEND_MODELS_APPLICATION_MODEL_H_
#define RDP_SOURCE_FRONTEND_MODELS_APPLICATION_MODEL_H_

#include <map>
#include <mutex>
#include <optional>
#include <vector>

#include <dd_common_api.h>

#include "application.h"
#include "blocklist_model.h"
#include "connection_model.h"
#include "models/api_model.h"
#include "tool_wrapper_types.h"

class QTabWidget;

struct DDConnectionInfo;
struct DDConnectionApi;

namespace rdp
{
    /// @brief The filtering mode for automatic application connections.
    enum ApplicationAutoConnectMode : uint8_t
    {
        kApplicationAutoConnectModeAnyApplication = 0,    ///< Any application that matches the API filter will automatically connect.
        kApplicationAutoConnectModeExistingApplications,  ///< Any application that is already in the application list will automatically connect.
        kApplicationAutoConnectModeNoApplications,        ///< No applications will automatically connect.
        kApplicationAutoConnectModeCount                  ///< The number of options.
    };

    /// @brief Application model representing all applications managed by the tool
    class ApplicationModel final : public QAbstractItemModel, public std::enable_shared_from_this<ApplicationModel>
    {
        Q_OBJECT
    public:
        /// @brief Describes the application table column categories
        enum ClientItemColumn
        {
            kClientItemColumnName,  ///< Application name
            kClientItemColumnCount
        };

        /// @brief Custom application data roles
        enum DataRoles : int32_t
        {
            kMutableDataRole = Qt::UserRole + 1,  ///< Used to access mutable data within model
        };

        /// @brief Constructor.
        /// @param [in] settings_manager Manages the RDP application settings.
        /// @param [in] connection_model The model used to manage connections to applications.
        /// @param [in] module_model The model that manages the models.
        /// @param [in] api_model The api model
        /// @param [in] parent The parent object.
        explicit ApplicationModel(std::shared_ptr<SettingsManager> settings_manager,
                                  std::weak_ptr<ConnectionModel>   connection_model,
                                  std::weak_ptr<ModuleModel>       module_model,
                                  std::weak_ptr<ApiModel>          api_model,
                                  QObject*                         parent = nullptr);

        /// @brief Destructor
        ~ApplicationModel() override;

        /// @brief Loads application list from settings
        void Load();

        /// @brief Saves application list to settings
        void Save();

        /// @brief Sets the auto connection mode.
        /// @param [in] mode The new auto connection mode.
        void SetAutoConnectionMode(ApplicationAutoConnectMode mode);

        /// @brief Sets the API filter.
        /// @param [in] api The new API filter.
        void SetApiFilter(ApiModel::Api api);

        /// @brief Gets the API filter.
        /// @return The api filter.
        ApiModel::Api GetApiFilter() const;

        /// @brief Gets the auto connect mode.
        /// @return auto connect mode.
        ApplicationAutoConnectMode GetAutoConnectMode() const;

        /// @brief Checks if any clients are connected
        /// @param [in] process_id The id of a process to ignore when figuring out if a clients are connected.
        /// @return true if any client connections active
        bool HasConnectedClients(DDProcessId process_id = -1);

        /// @brief Inserts an application entry into model
        /// @param [in] name The name of the application
        void AddApplication(const QString& name);

        /// @brief QAbstractListModel::data() override
        /// @param [in] index The index to query data for
        /// @param [in] role The data role
        /// @return variant data for specified role
        QVariant data(const QModelIndex& index, int role) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::flags() override
        /// @param [in] row The row to create index for
        /// @param [in] column The column to create index for
        /// @param [in] parent The parent of index
        /// @return new model index
        QModelIndex index(int row, int column, const QModelIndex& parent) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::parent() override
        /// @param [in] index The index to return parent for
        QModelIndex parent(const QModelIndex& index) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::flags() override
        /// @param [in] index The index to query flags for
        /// @return the item flags for index
        Qt::ItemFlags flags(const QModelIndex& index) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::rowCount() override
        /// @param [in] parent The parent index
        /// @return number of rows in list
        int rowCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::columnCount() override
        /// @param [in] parent The parent index
        /// @return number of rows in list
        int columnCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::setData() override
        /// @param [in] index The index to set internal data for
        /// @param [in] value The new data value
        /// @param [in] role The role to set new data for
        /// @return True if internal data changed
        bool setData(const QModelIndex& index, const QVariant& value, int role) Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::removeRows() override
        /// @param [in] row The row to start removal from
        /// @param [in] count The number of subsequent rows to remove
        /// @param [in] parent The parent index
        /// @return True is rows removed
        bool removeRows(int row, int count, const QModelIndex& parent) Q_DECL_OVERRIDE;

        /// @brief Gets the index for the application name if it exists.
        /// @param [in] name The application name.
        /// @return The index of the application. The index will be invalid if it is not found.
        QModelIndex IndexForName(const QString& name) const;

        /// @brief Provides the name for the application at the given index.
        /// @param [in] index The index to provide the name of the application for.
        QString NameForApplicationAtIndex(const QModelIndex& index) const;

        /// @brief Gets handle to application for specific index.
        /// @param [in] index The model index of the application to get.
        /// @return The application at the given index.
        std::shared_ptr<const Application> GetApplication(const QModelIndex& index) const;

        /// @brief Checks if any active clients are connected for the application with the given name.
        /// @param [in] name The name of the application to check.
        /// @return true if there are any active clients for the application.
        bool IsApplicationAlive(const QString& name) const;

        /// Sets the application that is displayed in module views.
        /// @param [in] index The name of the application to display module views.
        void SetModuleDisplayApplication(const QModelIndex& index) const;

    public slots:
        /// @brief Handle response to blocklist model loaded
        /// @param [in] model The blocklist model
        void OnBlocklistModelLoaded(const std::shared_ptr<BlocklistModel>& model);

        /// @brief Handle response to api model loaded
        void OnApiModelLoaded(const std::shared_ptr<ApiModel>& api_model);

    private slots:
        /// @brief Called when a module's enabled status changes.
        /// @param [in] module The module that changed.
        /// @param [in] is_enabled The new enabled status of the module.
        void OnModuleStatusChanged(const DevToolsModule* module, bool is_enabled);

        /// @brief Handle the blocklist updating one of its items.
        /// @param application_name The name of the item on the blocklist that was updated.
        void OnBlocklistItemChanged(const QString& application_name);

    signals:
        /// @brief Emitted when the API filter changes.
        /// @param [in] new_filter The new API filter.
        void ApiFilterChanged(ApiModel::Api new_filter);

        /// @brief Emitted when the auto connection mode changes.
        /// @param [in] mode The new auto connection mode.
        void AutoConnectModeChanged(ApplicationAutoConnectMode mode);

        /// @brief Emitted when trying to edit an application name, but the application name was on the blocklist.
        /// @param [in] app_name The application name.
        void EditedAppNameButAlreadyOnBlocklist(const QString& app_name);

        /// @brief Emitted when the connection behavior changes.
        /// @param [in] connection_behavior The new connection behavior string.
        void ConnectionBehaviorChanged(const QString& connection_behavior);

        /// @brief Signals the model has been initialized
        /// @param [in] model The model
        void Loaded(std::shared_ptr<ApplicationModel> model);

        /// @brief Signals client has connected for application entry
        /// @param [in] index The application entry index
        void ApplicationConnected(const QModelIndex& index);

        /// @brief Emitted when an application tries to connect, but an application is already connected.
        /// @param [in] app_name The name of the application that tried to connect.
        /// @param [in] pid The PID of the process.
        void ClientConnectedWhenAlreadyConnected(const QString& app_name, qint64 pid);

        /// @brief Signals application disconnected for application entry
        /// @param [in] index The application entry index
        void ApplicationDisconnected(const QModelIndex& index);

    private:
        /// @brief Determines if a client connection should be accepted or not.
        /// @param [in] userdata The model to use to check if the client should be ignored.
        /// @param [in] connection_info The connection info of the client.
        /// @return true if the client should be ignored, false otherwise.
        static bool ShouldClientBeIgnored(void* userdata, const DDConnectionInfo* connection_info);

        /// @brief Called when a client connects.
        /// @param [in] userdata The model to notify of the connection.
        /// @param [in] connection_info The connection info of the client.
        static void OnDriverConnected(DDConnectionCallbacksImpl* userdata, const DDConnectionInfo* connection_info);

        /// @brief Called when a client connects.
        /// @param [in] userdata The model to notify of the connection.
        /// @param [in] umd_connection_id The identifier of the UMD connection.
        static void OnDriverDisconnected(DDConnectionCallbacksImpl* userdata, DDConnectionId umd_connection_id);

        /// @brief Get application data for role
        /// @param [in] index The application index
        /// @param [in] role The data role
        /// @return application data for role
        QVariant ApplicationData(const QModelIndex& index, int role) const;
        /// @brief Gets the application data for display role
        /// @return display role data for application
        static QVariant GetApplicationDataForDisplayRole(const std::shared_ptr<const Application>& application, const QModelIndex& index);

        /// @brief Gets the application data for decoration role
        /// @param [in] application The application
        /// @param [in] index The model index
        /// @return decoration role data for application
        static QVariant GetApplicationDataForDecorationRole(const std::shared_ptr<const Application>& application, const QModelIndex& index);

        /// @brief Gets the application data for edit role
        /// @param [in] application The application
        /// @param [in] index The model index
        /// @return edit role data for application
        static QVariant GetApplicationDataForEditRole(const std::shared_ptr<const Application>& application, const QModelIndex& index);

        /// @brief Gets the application data for mutable role
        /// @param [in] application The application
        /// @param [in] index The model index
        /// @return mutable role data for application
        QVariant GetApplicationDataForMutableRole(const std::shared_ptr<const Application>& application, const QModelIndex& index) const;

        /// @brief Gets application (mutable) for specified name
        /// @param [in] name The name of the application
        /// @return application or null if none found
        std::shared_ptr<Application> FindApplicationForName(const QString& name) const;

        /// @brief Checks if model contains application with specified name
        /// @param [in] name The application name
        /// @return true if entry found, false otherwise
        bool HasEntryForName(const QString& name) const;

        /// @brief Returns true if the client should be filtered out because it is on the blocklist.
        /// @param [in] connection_info The new client connection information.
        /// @return true if the client should be filtered out, false otherwise.
        bool IsClientFilteredByBlocklist(const DDConnectionInfo& connection_info) const;

        /// @brief Returns true if the client should be filtered out because it would be disallowed by the auto connect mode.
        /// @param [in] connection_info The new client connection information.
        /// @return true if the client should be filtered out, false otherwise.
        bool IsClientFilteredByAutoConnectMode(const DDConnectionInfo& connection_info) const;

        /// @brief Called when a client connects.
        /// @param [in] connection_info The connection info of the client.
        void OnDriverConnected(const DDConnectionInfo& connection_info);

        /// @brief Called when a client connects.
        /// @param [in] umd_connection_id The identifier of the UMD connection.
        void OnDriverDisconnected(uint32_t umd_connection_id);

        /// @brief Gets a string that describes the connection behavior of the panel.
        /// @return A string that describes the connection behavior of the panel.
        QString GetRawConnectionBehaviorString();

        /// @brief Gets the string that describes all the API that match the filter, e.g. "DirectX 12 and Vulkan" or "Vulkan, HIP and OpenCL".
        /// @return The string that describes all the APIs that match the filter.
        QString GetApiFilterString() const;

        /// @brief Determines if a client connection should be accepted or not.
        /// @param [in] connection_info The connection info of the client.
        /// @return true if the client should be ignored, false otherwise.
        bool ShouldClientBeIgnored(const DDConnectionInfo& connection_info);

        std::vector<std::shared_ptr<Application>> applications_;              ///< collection of applications
        std::weak_ptr<BlocklistModel>             blocklist_model_;           ///< Blocklist model
        std::weak_ptr<ConnectionModel>            connection_model_;          ///< Model to manage application connections.
        std::weak_ptr<ApiModel>                   api_model_;                 ///< Api model
        std::weak_ptr<ModuleModel>                module_model_;              ///< The model that manages the modules.
        std::shared_ptr<SettingsManager>          settings_manager_;          ///< Manages the RDP application settings.
        std::optional<ApiModel::Api>              loaded_api_filter_;         ///< Loaded API filter.
        std::optional<ApplicationAutoConnectMode> loaded_auto_connect_mode_;  ///< Loaded auto connect mode.
        std::recursive_mutex                      connection_filter_mutex_;   ///< The mutex that guards all the things related to the connection filter.
        ApplicationAutoConnectMode                auto_connect_mode_ = kApplicationAutoConnectModeAnyApplication;  ///< The application auto connection filter.
        ApiModel::Api                             api_filter_ = ApiModel::Api::kWorkflowSupported;  ///< The API filter to apply to application connections.
        QString                                   conn_behavior_str_;                               ///< connection behavior string.

        DDConnectionApi* connection_api_ = nullptr;  ///< The API for managing driver connections.
    };

}  // namespace rdp

Q_DECLARE_METATYPE(std::shared_ptr<const rdp::Application>)
Q_DECLARE_METATYPE(std::shared_ptr<rdp::Application>)

#endif
