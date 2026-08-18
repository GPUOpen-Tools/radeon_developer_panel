// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP System info model class definition

#ifndef RDP_SOURCE_FRONTEND_MODELS_SYSTEM_INFO_MODEL_H_
#define RDP_SOURCE_FRONTEND_MODELS_SYSTEM_INFO_MODEL_H_

#include <cstdint>
#include <memory>

#include <QAbstractItemModel>
#include <QFormLayout>
#include <QJsonDocument>

#include <system_info_reader.h>

#include "bug/bug_report.h"

struct DDRouterUtilsApi;

namespace rdp
{
    class SystemInfoItem : public std::enable_shared_from_this<SystemInfoItem>
    {
    public:
        /// @brief Constructs system info item from key and value pair
        /// @param key The key for this item
        /// @param value The value for this item
        SystemInfoItem(QString key, QVariant value);

        /// @brief Destructor
        ~SystemInfoItem();

        /// @brief Adds a child item to this item and sets its parent
        /// @param item The child item to add
        /// @return pointer to child item
        std::shared_ptr<SystemInfoItem> AddChild(const std::shared_ptr<SystemInfoItem>& item);

        /// @brief Gets the key for this item
        /// @return key
        const QString& GetKey() const;

        /// @brief Gets the value for this item
        /// @return value
        const QVariant& GetValue() const;

        /// @brief Gets the parent for this item or nullptr if none
        /// @return parent or nullptr
        const SystemInfoItem* GetParent() const;

        /// @brief Gets the child specified by row
        /// @param row The row to access in child list
        /// @return child or nullptr if not found
        const SystemInfoItem* GetChild(int row) const;

        /// @brief Gets the row index for this item
        /// This method uses the parent to search its children
        /// list to find the appropriate child row index for
        /// this item.
        /// @return row index
        int GetRow() const;

        /// @brief Gets the child count
        /// @return child count
        int GetChildCount() const;

    private:
        QString                                      key_;       ///< Item key
        QVariant                                     value_;     ///< Item value
        SystemInfoItem*                              parent_;    ///< Parent item
        std::vector<std::shared_ptr<SystemInfoItem>> children_;  ///< Children
    };

    class SystemInfoModel final : public QAbstractItemModel, public std::enable_shared_from_this<SystemInfoModel>
    {
        Q_OBJECT
    public:
        /// @brief The different columns of this model.
        enum SystemInfoItemColumn
        {
            kSystemInfoItemColumnKey = 0,  ///< Key column
            kSystemInfoItemColumnValue,    ///< Value column
            kSystemInfoItemColumnCount     ///< The total number of columns
        };

        /// @brief Constructor
        /// @param [in] connection_model The model that handles the connections to RDS.
        /// @param parent The parent object
        explicit SystemInfoModel(const std::shared_ptr<class ConnectionModel>& connection_model, QObject* parent = nullptr);

        /// @brief Destructor
        ~SystemInfoModel() Q_DECL_NOEXCEPT Q_DECL_OVERRIDE;

    public slots:
        /// @brief Loads this model.
        void Load();

    private:
        /// @brief Loads the system info API from the registry if needed.
        /// @return true if the API is available, false otherwise.
        bool LoadSystemInfoApi();

        /// @brief Called when the system info is loaded.
        void PostSystemInfoLoad();

    public:
        /// @brief Returns true if the system info model has valid system info, false otherwise.
        /// @return true if the system info is valid, false otherwise.
        bool IsValid() const;

        /// @brief Gets if connected system is running Windows
        /// @return true if running Windows
        bool IsWindows() const;

        /// @brief Gets if connected system is running Linux
        /// @return true if running Linux
        bool IsLinux() const;

        /// @brief Gets the OS name
        /// @return OS name
        QString GetOsName() const;

        /// @brief Gets the OS description
        /// @return
        QString GetOsDescription() const;

        /// @brief Gets if ETW is supported
        /// @return true if ETW is enabled
        bool GetEtwIsSupported() const;

        /// @brief Gets if the AddUserToGroup batch file needs running
        /// @return true if needed, false otherwise
        bool GetAddUserToGroupScriptNeeded() const;

        /// @brief Gets if ETW has proper permissions
        /// @return true if ETW has permissions
        bool GetEtwHasPermission() const;

        /// @brief Gets the date of driver package
        /// @return packaging date if available, std::nullopt otherwise
        [[nodiscard]] std::optional<QDate> GetDriverPackagingDate() const;

        /// @brief Gets if the device is a vangogh or strix based handheld.
        /// @return true if device id and revision id of the hardware match handheld, false otherwise.
        [[nodiscard]] bool IsVanGoghOrStrixHandheld() const;

        /// @brief Gets the ETW status code
        /// @return status code
        ulong GetEtwStatusCode() const;

        /// @brief Gets the dynamic power manager writable flag
        /// @return true if writable
        bool IsPowerDpmWritable() const;

        /// @brief QAbstractListModel::data() override
        /// @param [in] index The index to query data for
        /// @param [in] role The data role
        /// @return variant data for specified role
        QVariant data(const QModelIndex& index, int role) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::headerData() override
        /// @param [in] section The header section to return data for
        /// @param [in] orientation The orientation state of header
        /// @param [in] role The data role
        /// @return data for header
        QVariant headerData(int section, Qt::Orientation orientation, int role) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::flags() override
        /// @param [in] row The row to create index for
        /// @param [in] column The column to create index for
        /// @param [in] parent The parent of index
        /// @return new model index
        QModelIndex index(int row, int column, const QModelIndex& parent) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::parent() override
        /// @param [in] index The index to return parent for
        QModelIndex parent(const QModelIndex& index) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::rowCount() override
        /// @param [in] parent The parent index
        /// @return number of rows in list
        int rowCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::columnCount() override
        /// @param [in] parent The parent index
        /// @return number of rows in list
        int columnCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;

        /// @brief Dumps the full system info JSON to the specified path.
        /// @param output_path The path of the folder to dump the data to.
        /// @return true if write was successful, false otherwise.
        bool ExportInfo(const QString& output_path) const;

        /// @brief Fills the bug report with all the connected system information.
        /// @param report The report to fill with the system information.
        void FillBugReport(BugReport& report) const;

        /// @brief Builds and returns a form layout for system info display.
        /// @return The form layout for display of system info.
        QFormLayout* GetFormLayout() const;

    signals:
        /// @brief Signal system info model loaded
        /// @param model The system info model handle
        void Loaded(std::shared_ptr<SystemInfoModel> model);

        /// @brief Emitted when the validity of the data in the model changes.
        /// @param is_valid true if the data in the model is valid, false otherwise.
        void IsDataValidChanged(bool is_valid);

        /// @brief Called when the current system information changes for any reason.
        /// @param system_info_str The system information as a JSON string.
        void SystemInfoChanged(const QString& system_info_str);

        /// @brief Emitted when the system info fails to load.
        ///
        /// This is a critical error and should trigger application exit.
        void SystemInfoFailedToLoad();

        /// @brief Emitted when the system info model loads if the ETW status code was nonzero.
        /// @param [in] has_permission true if the permissions to use ETW were granted, false otherwise.
        void EtwStatusCodeNonzero(bool has_permission);

        /// @brief Emitted when the 'AddUserToGroup' batch file needs to be run for sync primitives
        void AddUserToGroupBatNeeded();

        /// @brief Emitted when there is an unsupported driver in use with handheld device.
        void UnsupportedHandheldDriver();

    public slots:

        /// @brief Called when the net bus disconnects and clears the system information.
        void NetBusDisconnect();

    private:
        /// @brief Formats the value as a hex string without the 0x prefix.
        /// @param [in] value The value to format.
        /// @return A string representation of the value in hex without the 0x prefix.
        static QString FormatHex(uint32_t value);

        /// @brief Queries the system info from router module
        /// @param user_data The handle to system info model
        /// @param text The system info JSON string
        static void QuerySystemInfoJsonCallback(void* user_data, const char* text);

        std::shared_ptr<ConnectionModel> connection_model_;            ///< The model that manages the connection to RDS.
        system_info_utils::SystemInfo    info_;                        ///< The parsed system information.
        std::unique_ptr<SystemInfoItem>  root_;                        ///< The data for the rows of this model.
        QJsonDocument                    json_document_;               ///< The JSON data as document
        QString                          json_data_;                   ///< The raw JSON data that contains the system information.
        bool                             has_info_         = false;    ///< true if this model has the system info for a connected system.
        DDRouterUtilsApi*                router_utils_api_ = nullptr;  ///< The API to use to get system info.
    };
}  // namespace rdp

#endif
