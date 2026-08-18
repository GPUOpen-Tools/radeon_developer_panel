// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Blocked Applications model definition

#ifndef RDP_SOURCE_FRONTEND_MODELS_BLOCKLIST_MODEL_H_
#define RDP_SOURCE_FRONTEND_MODELS_BLOCKLIST_MODEL_H_

#include <map>
#include <memory>

#include "settings_manager.h"
#include "system_info_model.h"
#include "tool_wrapper_types.h"

namespace rdp
{
    /// @brief Blocklist QAbstractListModel subclass
    class BlocklistModel final : public QAbstractListModel, public std::enable_shared_from_this<BlocklistModel>
    {
    public:
        enum class Platform : uint32_t
        {
            kUnknown = 0,
            kWindows,
            kLinux,
            kCount
        };

        Q_OBJECT
    public:
        /// @brief Constructor
        /// @param [in] connection_model The model that manages the connection to RDS.
        /// @param [in] settings_manager The object used to manage RDP application settings.
        /// @param [in] parent The parent object
        explicit BlocklistModel(const std::shared_ptr<ConnectionModel>& connection_model,
                                std::shared_ptr<SettingsManager>        settings_manager,
                                QObject*                                parent = nullptr);

        /// @brief Destructor
        ~BlocklistModel() Q_DECL_OVERRIDE;

        /// @brief Loads the blocklist from settings
        void Load();

        /// @brief Saves the blocklist to settings
        void Save();

        /// @brief Should be called when a model binds to this model.
        void OnBind();

    public slots:
        /// @brief Restores model to hardcoded blocklist defaults
        void RestoreDefaults();

    public:
        /// @brief Adds an application to the blocklist.
        /// @param [in] name The name of the application.
        void AddApplication(const QString& name);

        /// @brief Checks if specified name is in blocklist
        /// @param [in] name The name
        /// @return true if blocked, false otherwise
        bool Contains(const QString& name) const;

    private:
        /// @brief Checks if specified name is in blocklist for the platform.
        /// @param [in] name The name.
        /// @param [in] platform The platform to check.
        /// @return true if blocked, false otherwise
        bool Contains(const QString& name, Platform platform) const;

    public:
        /// @brief QAbstractListModel::data() implementation
        /// @param [in] index The index to query data for
        /// @param [in] role The data role
        /// @return variant data for specified role
        QVariant data(const QModelIndex& index, int role) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::flags() implementation
        /// @param [in] index The index to query flags for
        /// @return the item flags for index
        Qt::ItemFlags flags(const QModelIndex& index) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::rowCount() implementation
        /// @param [in] parent The parent index
        /// @return number of rows in list
        int rowCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::setData() implementation
        /// @param [in] index The index to set internal data for
        /// @param [in] value The new data value
        /// @param [in] role The role to set new data for
        /// @return True if internal data changed
        bool setData(const QModelIndex& index, const QVariant& value, int role) Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::removeRows() implementation
        /// @param [in] row The row to start removal from
        /// @param [in] count The number of subsequent rows to remove
        /// @param [in] parent The parent index
        /// @return True is rows removed
        bool removeRows(int row, int count, const QModelIndex& parent) Q_DECL_OVERRIDE;

        /// @brief QAbstractListModel::insertRows() implementation
        /// @param [in] row The row to start insert from
        /// @param [in] count The number of rows to insert
        /// @param [in] parent The parent index
        /// @return True is rows inserted
        bool insertRows(int row, int count, const QModelIndex& parent) Q_DECL_OVERRIDE;

    signals:
        /// @brief Signal model finished loading
        void Loaded(std::shared_ptr<BlocklistModel> blocklist_model);

        /// @brief Signal for when the name of an item on the blocklist is changed.
        /// @param application_name The changed name of the application on the blocklist.
        void OnBlocklistItemChanged(const QString& application_name);

        /// @brief Emitted when the platform changes.
        /// @param [in] new_platform The new platform for the blocklist model.
        void PlatformChanged(Platform new_platform);

    public slots:
        /// @brief Handle response to system info model loaded
        /// @param [in] model The system info model
        void OnSystemInfoModelLoaded(const std::shared_ptr<SystemInfoModel>& model);

        /// @brief Called when the connection state changes to disconnecting.
        void NetDisconnecting();

    private:
        /// @brief Utility method to get group name string from platform type
        /// @param platform The platform
        /// @return string name for platform settings group
        static QString GroupNameFromPlatform(Platform platform);

        std::map<Platform, QStringList>   blocklists_;                     ///< platform blocklist map
        std::map<Platform, QStringList>   default_blocklist_;              ///< The hard-coded default blocklist.
        std::map<Platform, QSet<QString>> removed_default_blocklist_;      ///< The entries from default list removed by user.
        Platform                          platform_ = Platform::kUnknown;  ///< current platform
        std::shared_ptr<SettingsManager>  settings_manager_;               ///< Manages the RDP application settings.
    };
}  // namespace rdp

#endif
