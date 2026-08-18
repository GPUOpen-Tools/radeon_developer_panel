// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Managed Apps Model class implementation

#include "application_model.h"

#include <algorithm>
#include <utility>

#include <QByteArray>
#include <QSettings>

#include <dd_connection_api.h>

#include <dev_trace_common.h>

#include <common/inc/api/dev_tools_module.h>

#include "logging/logging_manager.h"
#include "mainwindow.h"

namespace rdp
{
    static constexpr auto kSettingAutoConnectMode      = "autoConnectMode";
    static constexpr auto kSettingApiFilter            = "apiFilter";
    static constexpr auto kEnabledModulesSupportString = "an API supported by the currently enabled features";

    bool ApplicationModel::ShouldClientBeIgnored(void* userdata, const DDConnectionInfo* connection_info)
    {
        const auto model = static_cast<ApplicationModel*>(userdata);
        return model->ShouldClientBeIgnored(*connection_info);
    }

    void ApplicationModel::OnDriverConnected(DDConnectionCallbacksImpl* userdata, const DDConnectionInfo* connection_info)
    {
        const auto model = reinterpret_cast<ApplicationModel*>(userdata);
        model->OnDriverConnected(*connection_info);
    }

    void ApplicationModel::OnDriverDisconnected(DDConnectionCallbacksImpl* userdata, const DDConnectionId umd_connection_id)
    {
        const auto model = reinterpret_cast<ApplicationModel*>(userdata);
        model->OnDriverDisconnected(umd_connection_id);
    }

    ApplicationModel::ApplicationModel(std::shared_ptr<SettingsManager> settings_manager,
                                       std::weak_ptr<ConnectionModel>   connection_model,
                                       std::weak_ptr<ModuleModel>       module_model,
                                       std::weak_ptr<ApiModel>          api_model,
                                       QObject*                         parent)
        : QAbstractItemModel(parent)
        , connection_model_(std::move(connection_model))
        , api_model_(std::move(api_model))
        , module_model_(std::move(module_model))
        , settings_manager_(std::move(settings_manager))
    {
        auto api_model_locked        = api_model_.lock();
        auto connection_model_locked = connection_model_.lock();
        auto module_model_locked     = module_model_.lock();
        if (connection_model_locked == nullptr || api_model_locked == nullptr || module_model_locked == nullptr)
        {
            return;
        }

        connect(api_model_locked.get(), &ApiModel::Loaded, this, &ApplicationModel::OnApiModelLoaded);
        connect(module_model_locked.get(), &ModuleModel::ModuleStatusChanged, this, &ApplicationModel::OnModuleStatusChanged);

        DDApiRegistry* api_registry = connection_model_locked->GetApiRegistry();
        if (api_registry == nullptr)
        {
            return;
        }

        const DD_RESULT api_get_result =
            api_registry->Get(api_registry->pInstance,
                              DD_CONNECTION_API_NAME,
                              DDVersion{DD_CONNECTION_API_VERSION_MAJOR, DD_CONNECTION_API_VERSION_MINOR, DD_CONNECTION_API_VERSION_PATCH},
                              reinterpret_cast<void**>(&connection_api_));
        if (api_get_result != DD_RESULT_SUCCESS)
        {
            return;
        }

        DDConnectionFilter connection_filter{};
        connection_filter.pUserData = this;
        connection_filter.filter    = &ApplicationModel::ShouldClientBeIgnored;
        connection_api_->SetConnectionFilter(connection_api_->pInstance, connection_filter);

        DDConnectionCallbacks connection_callbacks{};
        connection_callbacks.pImpl                = reinterpret_cast<DDConnectionCallbacksImpl*>(this);
        connection_callbacks.OnDriverConnected    = &ApplicationModel::OnDriverConnected;
        connection_callbacks.OnDriverDisconnected = &ApplicationModel::OnDriverDisconnected;
        connection_api_->AddConnectionCallbacks(connection_api_->pInstance, &connection_callbacks);
    }

    ApplicationModel::~ApplicationModel()
    {
        if (connection_api_ != nullptr)
        {
            DDConnectionFilter null_filter{};
            null_filter.pUserData = nullptr;
            null_filter.filter    = nullptr;
            connection_api_->SetConnectionFilter(connection_api_->pInstance, null_filter);

            connection_api_->RemoveConnectionCallbacks(connection_api_->pInstance, reinterpret_cast<DDConnectionCallbacksImpl*>(this));
        }

        applications_.clear();
    }

    void ApplicationModel::OnBlocklistModelLoaded(const std::shared_ptr<BlocklistModel>& model)
    {
        blocklist_model_ = model;
        connect(model.get(), &BlocklistModel::OnBlocklistItemChanged, this, &ApplicationModel::OnBlocklistItemChanged);
    }

    void ApplicationModel::OnApiModelLoaded(const std::shared_ptr<ApiModel>& api_model)
    {
        if (loaded_api_filter_.has_value())
        {
            if (const auto value = loaded_api_filter_.value(); api_model->IsSupported(value))
            {
                SetApiFilter(value);
            }

            loaded_api_filter_.reset();
        }

        if (loaded_auto_connect_mode_.has_value())
        {
            const auto value = loaded_auto_connect_mode_.value();
            SetAutoConnectionMode(value);

            loaded_auto_connect_mode_.reset();
        }

        const std::lock_guard connection_filter_lock(connection_filter_mutex_);
        conn_behavior_str_ = GetRawConnectionBehaviorString();

        emit ConnectionBehaviorChanged(conn_behavior_str_);
    }

    void ApplicationModel::AddApplication(const QString& name)
    {
        const QString trimmed_name = name.trimmed();
        if (trimmed_name.isEmpty())
        {
            return;
        }

        if (FindApplicationForName(trimmed_name) != nullptr)
        {
            return;
        }

        if (const auto blocklist_model = blocklist_model_.lock(); blocklist_model != nullptr)
        {
            if (blocklist_model->Contains(trimmed_name))
            {
                return;
            }
        }

        auto application = std::make_shared<Application>();
        application->SetName(trimmed_name);

        const int num_rows_before_insert = rowCount({});
        beginInsertRows({}, num_rows_before_insert, num_rows_before_insert);

        applications_.push_back(std::move(application));

        endInsertRows();
        Save();
    }

    void ApplicationModel::Load()
    {
        beginResetModel();

        RDP_LOG_INFO("Loading settings group: [{}]", "Managed Applications");

        // Load blocklist settings from settings.ini if available
        QSettings* settings = settings_manager_->GetSettings();
        Q_ASSERT(settings != nullptr);

        if (const QStringList groups = settings->childGroups(); groups.contains("Managed Applications"))
        {
            settings->beginGroup("Managed Applications");

            const int size = settings->beginReadArray("applications");
            for (int i = 0; i < size; i++)
            {
                settings->setArrayIndex(i);

                const QString name = settings->value("name").toString();
                applications_.push_back(std::make_shared<Application>(name));
            }
            settings->endArray();

            loaded_api_filter_ = static_cast<ApiModel::Api>(settings->value(kSettingApiFilter).toInt());

            loaded_auto_connect_mode_ = static_cast<ApplicationAutoConnectMode>(settings->value(kSettingAutoConnectMode).toInt());

            settings->endGroup();
        }

        endResetModel();

        emit Loaded(shared_from_this());
    }

    void ApplicationModel::Save()
    {
        RDP_LOG_INFO("Saving settings group: [{}]", "Managed Applications");

        // Save blocklist settings from settings.ini if available
        QSettings* settings = settings_manager_->GetSettings();
        Q_ASSERT(settings != nullptr);

        settings->beginGroup("Managed Applications");
        settings->beginWriteArray("applications");

        for (size_t i = 0; i < applications_.size(); i++)
        {
            const auto& application = applications_[i];
            Q_ASSERT(application != nullptr);

            settings->setArrayIndex(static_cast<int>(i));
            settings->setValue("name", application->GetName());
        }

        settings->endArray();

        settings->setValue(kSettingApiFilter, static_cast<int>(api_filter_));
        settings->setValue(kSettingAutoConnectMode, auto_connect_mode_);
        settings->endGroup();
    }

    void ApplicationModel::SetAutoConnectionMode(const ApplicationAutoConnectMode mode)
    {
        const std::lock_guard connection_filter_lock(connection_filter_mutex_);
        auto_connect_mode_ = mode;

        Save();

        emit AutoConnectModeChanged(auto_connect_mode_);

        conn_behavior_str_ = GetRawConnectionBehaviorString();
        emit ConnectionBehaviorChanged(conn_behavior_str_);
    }

    void ApplicationModel::SetApiFilter(const ApiModel::Api api)
    {
        if (const auto api_model = api_model_.lock(); api_model == nullptr || !api_model->IsSupported(api))
        {
            return;
        }

        const std::lock_guard connection_filter_lock(connection_filter_mutex_);
        api_filter_ = api;

        Save();

        emit ApiFilterChanged(api_filter_);

        conn_behavior_str_ = GetRawConnectionBehaviorString();
        emit ConnectionBehaviorChanged(conn_behavior_str_);
    }

    void ApplicationModel::OnModuleStatusChanged(const DevToolsModule* module, const bool is_enabled)
    {
        Q_UNUSED(module);

        const auto module_model = module_model_.lock();
        if (module_model == nullptr)
        {
            return;
        }

        const std::lock_guard connection_filter_lock(connection_filter_mutex_);
        if (!is_enabled && !module_model->IsApiSupportedByEnabledModules(static_cast<devtrace::Api>(api_filter_)))
        {
            api_filter_ = ApiModel::Api::kWorkflowSupported;
            emit ApiFilterChanged(api_filter_);
        }

        conn_behavior_str_ = GetRawConnectionBehaviorString();
        emit ConnectionBehaviorChanged(conn_behavior_str_);
    }

    bool ApplicationModel::ShouldClientBeIgnored(const DDConnectionInfo& connection_info)
    {
        const std::lock_guard connection_filter_lock(connection_filter_mutex_);
        const auto            api_model    = api_model_.lock();
        const auto            module_model = module_model_.lock();

        if (api_model == nullptr || module_model == nullptr)
        {
            return true;
        }

        const char* process_name       = connection_info.pProcessName;
        const char* driver_description = connection_info.pDescription;

        if (!module_model->HasModulesNeedingConnections())
        {
            RDP_LOG_INFO("Ignoring client: {} [{}] because no modules currently require per-process connections", process_name, driver_description);
            return true;
        }

        const ApiModel::Api api          = api_model->GetApiFromDriverDescription(connection_info.pDescription);
        const auto          devtrace_api = static_cast<devtrace::Api>(api);
        const QString       api_name     = api_model->GetName(api);

        if (!api_model->IsSupported(api))
        {
            RDP_LOG_INFO("Ignoring client: {} [{}] using unsupported API", process_name, qUtf8Printable(api_name));
            return true;
        }

        if (const bool filter_is_workflow_supported = api_filter_ == ApiModel::Api::kWorkflowSupported;
            (filter_is_workflow_supported && !module_model->IsApiSupportedByEnabledModules(devtrace_api)) ||
            (!filter_is_workflow_supported && api != api_filter_))
        {
            RDP_LOG_INFO("Ignoring client: {} [{}] because it does not match the API filter", process_name, qUtf8Printable(api_name));
            return true;
        }

        if (IsClientFilteredByBlocklist(connection_info))
        {
            RDP_LOG_INFO("Ignoring client: {} [{}] because it is on the blocklist", process_name, qUtf8Printable(api_name));
            return true;
        }

        if (IsClientFilteredByAutoConnectMode(connection_info))
        {
            RDP_LOG_INFO("Ignoring client: {} [{}] because it does not fit the auto connection filter", process_name, qUtf8Printable(api_name));
            return true;
        }

        if (HasConnectedClients(connection_info.processId))
        {
            emit ClientConnectedWhenAlreadyConnected(process_name, connection_info.processId);

            RDP_LOG_INFO("Ignoring client: {} [{}] because another client was already connected", process_name, driver_description);
            return true;
        }

        return false;
    }

    bool ApplicationModel::HasConnectedClients(const DDProcessId process_id)
    {
        const std::lock_guard connection_filter_lock(connection_filter_mutex_);
        for (int i = 0; i < rowCount(QModelIndex()); i++)
        {
            const auto& application = applications_.at(i);
            Q_ASSERT(application != nullptr);

            if (application->IsAlive() && application->GetProcessId() != process_id)
            {
                return true;
            }
        }

        return false;
    }

    bool ApplicationModel::IsClientFilteredByBlocklist(const DDConnectionInfo& connection_info) const
    {
        const auto blocklist_model = blocklist_model_.lock();
        if (blocklist_model == nullptr)
        {
            return true;
        }

        const bool is_blocked_by_blocklist = blocklist_model->Contains(connection_info.pProcessName);
        if (is_blocked_by_blocklist)
        {
            RDP_LOG_INFO("Client [{}] exists in blocklist, ignoring connection", connection_info.pProcessName);
        }

        return is_blocked_by_blocklist;
    }

    bool ApplicationModel::IsClientFilteredByAutoConnectMode(const DDConnectionInfo& connection_info) const
    {
        switch (auto_connect_mode_)
        {
        case kApplicationAutoConnectModeAnyApplication:
            return false;
        case kApplicationAutoConnectModeExistingApplications:
            return !HasEntryForName(connection_info.pProcessName);
        case kApplicationAutoConnectModeNoApplications:
            return true;
        default:
            return false;
        }
    }

    void ApplicationModel::OnDriverConnected(const DDConnectionInfo& connection_info)
    {
        const std::lock_guard connection_filter_lock(connection_filter_mutex_);

        // If the application already exists it won't be added.
        AddApplication(connection_info.pProcessName);

        const std::shared_ptr<Application> application = FindApplicationForName(connection_info.pProcessName);
        Q_ASSERT(application != nullptr);

        const bool was_connected = application->IsAlive();
        application->ClientConnected(connection_info);

        if (!was_connected)
        {
            const QModelIndex app_index = IndexForName(connection_info.pProcessName).siblingAtColumn(kClientItemColumnName);
            emit              dataChanged(app_index, app_index);
            emit              ApplicationConnected(app_index);

            if (const auto module_model = module_model_.lock(); module_model != nullptr)
            {
                module_model->LockModules();
            }
        }
    }

    void ApplicationModel::OnDriverDisconnected(const uint32_t umd_connection_id)
    {
        const std::lock_guard connection_filter_lock(connection_filter_mutex_);
        for (int i = 0; i < static_cast<int>(applications_.size()); ++i)
        {
            const std::shared_ptr<Application>& application   = applications_[i];
            const bool                          was_connected = application->IsAlive();
            application->ClientDisconnected(umd_connection_id);

            if (was_connected && !application->IsAlive())
            {
                const QModelIndex app_index = index(i, kClientItemColumnName, {});
                emit              dataChanged(app_index, app_index);
                emit              ApplicationDisconnected(app_index);

                if (const auto module_model = module_model_.lock(); module_model != nullptr)
                {
                    module_model->UnlockModules();
                }
            }
        }
    }

    QString ApplicationModel::GetRawConnectionBehaviorString()
    {
        const auto module_model     = module_model_.lock();
        const auto connection_model = connection_model_.lock();
        if (module_model == nullptr || connection_model == nullptr)
        {
            return "";
        }

        if (!connection_model->IsConnected())
        {
            return "The panel will not connect to any application until it is connected to a system.";
        }

        if (!module_model->HasModulesNeedingConnections())
        {
            return "The panel will not connect to any application until a feature that requires application connections is enabled.";
        }

        const std::lock_guard connection_filter_lock(connection_filter_mutex_);
        const QString         api_filter_string = GetApiFilterString();

        switch (auto_connect_mode_)
        {
        case kApplicationAutoConnectModeAnyApplication:
            return QString("The panel will automatically connect to an application that is using %1.").arg(api_filter_string);
        case kApplicationAutoConnectModeExistingApplications:
            return QString("The panel will automatically connect to an application that exists in the application list and is using %1.")
                .arg(api_filter_string);
        case kApplicationAutoConnectModeNoApplications:
            return QString("The panel will only connect to an application launched from the panel that is using %1.").arg(api_filter_string);
        default:
            return "";
        }
    }

    QString ApplicationModel::GetApiFilterString() const
    {
        const auto api_model    = api_model_.lock();
        const auto module_model = module_model_.lock();

        if (api_model == nullptr || module_model == nullptr)
        {
            return "";
        }

        if (api_filter_ != ApiModel::Api::kWorkflowSupported)
        {
            return api_model->GetName(api_filter_);
        }

        QStringList supported_apis;
        for (uint32_t i = static_cast<uint32_t>(ApiModel::Api::kUnknown) + 1; i < static_cast<uint32_t>(ApiModel::Api::kCount); i++)
        {
            const auto api = static_cast<devtrace::Api>(i);
            if (const auto model_api = static_cast<ApiModel::Api>(i); api_model->IsSupported(model_api) && module_model->IsApiSupportedByEnabledModules(api))
            {
                supported_apis.push_back(api_model->GetName(model_api));
            }
        }

        if (supported_apis.empty())
        {
            return kEnabledModulesSupportString;
        }

        QString api_list_string = supported_apis[0];
        if (supported_apis.size() > 1)
        {
            const QString last_api = supported_apis.last();
            supported_apis.removeLast();

            api_list_string = QString("%1 and %2").arg(supported_apis.join(", "), last_api);
        }

        return QString("%1 (%2)").arg(kEnabledModulesSupportString, api_list_string);
    }

    int ApplicationModel::columnCount([[maybe_unused]] const QModelIndex& parent) const
    {
        return kClientItemColumnCount;
    }

    bool ApplicationModel::removeRows(const int row, const int count, const QModelIndex& parent)
    {
        for (int app = row; app < row + count; ++app)
        {
            if (applications_[app]->IsAlive())
            {
                return false;
            }
        }

        // Only remove if row is specified
        if (row >= 0)
        {
            const int last = row + (count - 1);
            beginRemoveRows(parent, row, last);

            for (int i = row; i <= last; i++)
            {
                applications_.erase(applications_.begin() + i);
            }

            endRemoveRows();

            // Save model
            Save();

            return true;
        }

        return false;
    }

    std::shared_ptr<Application> ApplicationModel::FindApplicationForName(const QString& name) const
    {
        const auto iterator = std::ranges::find_if(applications_, [name](const std::shared_ptr<Application>& app) { return app->GetName() == name; });
        return iterator != applications_.end() ? *iterator : nullptr;
    }

    bool ApplicationModel::HasEntryForName(const QString& name) const
    {
        return FindApplicationForName(name) != nullptr;
    }

    void ApplicationModel::OnBlocklistItemChanged(const QString& application_name)
    {
        if (const QModelIndex index = IndexForName(application_name); index.isValid())
        {
            const auto& application = applications_.at(index.row());
            Q_ASSERT(application != nullptr);
            if (!application->IsAlive())
            {
                removeRow(index.row());
            }
            else
            {
                RDP_LOG_ERROR("Application was added to the blocklist, but is alive {}", qUtf8Printable(application_name));
            }
        }
    }

    QModelIndex ApplicationModel::IndexForName(const QString& name) const
    {
        const int num_rows = rowCount(QModelIndex());
        for (int i = 0; i < num_rows; i++)
        {
            const auto& application = applications_.at(i);
            Q_ASSERT(application != nullptr);
#ifdef Q_OS_WIN
            constexpr Qt::CaseSensitivity case_sensitivity = Qt::CaseInsensitive;
#else
            constexpr Qt::CaseSensitivity case_sensitivity = Qt::CaseSensitive;
#endif
            if (name.compare(application->GetName(), case_sensitivity) == 0)
            {
                return createIndex(i, 0);
            }
        }

        return {};
    }

    QString ApplicationModel::NameForApplicationAtIndex(const QModelIndex& index) const
    {
        if (!index.isValid())
        {
            return {};
        }

        if (!index.parent().isValid())
        {
            return applications_[index.row()]->GetName();
        }

        const auto* application = static_cast<const Application*>(index.internalPointer());
        return application->GetName();
    }

    std::shared_ptr<const Application> ApplicationModel::GetApplication(const QModelIndex& index) const
    {
        Q_ASSERT(index.isValid());
        return data(index, Qt::UserRole).value<std::shared_ptr<const Application>>();
    }

    bool ApplicationModel::IsApplicationAlive(const QString& name) const
    {
        if (const QModelIndex index = IndexForName(name); index.isValid())
        {
            const auto& application = applications_.at(index.row());
            Q_ASSERT(application != nullptr);
            return application->IsAlive();
        }

        return false;
    }

    void ApplicationModel::SetModuleDisplayApplication(const QModelIndex& index) const
    {
        const auto module_model = module_model_.lock();
        if (module_model == nullptr)
        {
            return;
        }

        if (applications_.empty())
        {
            module_model->SetDisplayApplication("");
            return;
        }

        const auto& application = index.isValid() ? applications_.at(index.row()) : applications_.front();
        Q_ASSERT(application != nullptr);

        module_model->SetDisplayApplication(application->GetName());
    }

    int ApplicationModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid())
        {
            return 0;
        }

        return static_cast<int>(applications_.size());
    }

    QModelIndex ApplicationModel::parent(const QModelIndex& index) const
    {
        Q_UNUSED(index);
        return {};
    }

    QModelIndex ApplicationModel::index(const int row, const int column, const QModelIndex& parent) const
    {
        if (!hasIndex(row, column, parent) || parent.isValid())
        {
            return {};
        }

        return createIndex(row, column);
    }

    Qt::ItemFlags ApplicationModel::flags(const QModelIndex& index) const
    {
        if (!index.isValid() || index.parent().isValid())
        {
            return QAbstractItemModel::flags(index);
        }

        if (const auto& application = applications_[index.row()]; index.column() == kClientItemColumnName && !application->IsAlive())
        {
            return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
        }

        return QAbstractItemModel::flags(index);
    }

    QVariant ApplicationModel::GetApplicationDataForDisplayRole(const std::shared_ptr<const Application>& application, const QModelIndex& index)
    {
        Q_ASSERT(index.isValid());
        Q_ASSERT(application != nullptr);

        if (index.column() == kClientItemColumnName)
        {
            return application->GetName();
        }

        return {};
    }

    QVariant ApplicationModel::GetApplicationDataForDecorationRole(const std::shared_ptr<const Application>& application, const QModelIndex& index)
    {
        Q_ASSERT(index.isValid());
        Q_ASSERT(application != nullptr);

        if (index.column() == kClientItemColumnName && application->IsAlive())
        {
            return QIcon(":/green_light.svg");
        }

        return {};
    }

    QVariant ApplicationModel::GetApplicationDataForEditRole(const std::shared_ptr<const Application>& application, const QModelIndex& index)
    {
        Q_ASSERT(application != nullptr);
        Q_ASSERT(index.isValid());

        if (index.column() == kClientItemColumnName)
        {
            return application->GetName();
        }

        return {};
    }

    QVariant ApplicationModel::GetApplicationDataForMutableRole(const std::shared_ptr<const Application>& application, const QModelIndex& index) const
    {
        Q_UNUSED(application)
        Q_ASSERT(application != nullptr);
        Q_ASSERT(index.isValid());

        QVariant variant;

        if (const int column = index.column(); column == kClientItemColumnName)
        {
            variant = QVariant::fromValue(applications_[index.row()]);
        }

        return variant;
    }

    QVariant ApplicationModel::ApplicationData(const QModelIndex& index, const int role) const
    {
        QVariant variant;

        if (index.isValid())
        {
            const std::shared_ptr<const Application> application = applications_[index.row()];
            // If we get an invalid app info pointer it means our indices are invalid somehow
            Q_ASSERT(application != nullptr);

            if (role == Qt::DisplayRole)
            {
                variant = GetApplicationDataForDisplayRole(application, index);
            }

            if (role == Qt::DecorationRole)
            {
                variant = GetApplicationDataForDecorationRole(application, index);
            }

            if (role == Qt::UserRole)
            {
                variant = QVariant::fromValue<std::shared_ptr<const Application>>(application);
            }

            if (role == Qt::EditRole)
            {
                variant = GetApplicationDataForEditRole(application, index);
            }

            if (role == kMutableDataRole)
            {
                variant = GetApplicationDataForMutableRole(application, index);
            }
        }

        return variant;
    }

    QVariant ApplicationModel::data(const QModelIndex& index, const int role) const
    {
        if (!index.isValid() || applications_.empty() || index.parent().isValid())
        {
            return {};
        }

        return ApplicationData(index, role);
    }

    bool ApplicationModel::setData(const QModelIndex& index, const QVariant& value, const int role)
    {
        if (index.isValid())
        {
            const auto& application = applications_.at(index.row());
            switch (role)
            {
            case Qt::EditRole:
            {
                if (index.column() == kClientItemColumnName)
                {
                    const QString trimmed_value = value.toString().trimmed();
                    if (trimmed_value.isEmpty())
                    {
                        return false;
                    }

                    if (const auto blocklist_model = blocklist_model_.lock(); blocklist_model != nullptr)
                    {
                        if (blocklist_model->Contains(trimmed_value))
                        {
                            emit EditedAppNameButAlreadyOnBlocklist(trimmed_value);
                            return false;
                        }
                    }

                    // Check if there is already an existing entry with this name
                    if (!HasEntryForName(trimmed_value))
                    {
                        application->SetName(trimmed_value);
                        emit dataChanged(index, index);
                    }
                }

                // IMPORTANT:
                // Save immediately for standard EditRole emitted through
                // item delegate.
                Save();

                return true;
            }

            default:
                break;
            }
        }

        return false;
    }

    ApiModel::Api ApplicationModel::GetApiFilter() const
    {
        return api_filter_;
    }

    ApplicationAutoConnectMode ApplicationModel::GetAutoConnectMode() const
    {
        return auto_connect_mode_;
    }

}  // namespace rdp
