// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Blocked Applications model implementation

#include "blocklist_model.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>

#include "connection_model.h"
#include "definitions.h"
#include "logging/logging_manager.h"

static constexpr auto kBlocklistGroup       = "Blocklist";                 ///< QSettings blocklist group prefix
static constexpr auto kRemovedDefaultsGroup = "BlocklistRemovedDefaults";  ///< QSettings removed default blocklist items group prefix.
static constexpr auto kWinGroup             = "Win32";                     ///< Win32 blocklist prefix
static constexpr auto kLinuxGroup           = "Linux";                     ///< Linux blocklist prefix

namespace rdp
{
    QString BlocklistModel::GroupNameFromPlatform(const Platform platform)
    {
        switch (platform)
        {
        case Platform::kWindows:
            return kWinGroup;

        case Platform::kLinux:
            return kLinuxGroup;

        default:
            break;
        }

        return "Unknown";
    }

    BlocklistModel::BlocklistModel(const std::shared_ptr<ConnectionModel>& connection_model, std::shared_ptr<SettingsManager> settings_manager, QObject* parent)
        : QAbstractListModel(parent)
        , settings_manager_(std::move(settings_manager))
    {
        for (uint32_t index = static_cast<uint32_t>(Platform::kUnknown) + 1; index < static_cast<uint32_t>(Platform::kCount); index++)
        {
            blocklists_[static_cast<Platform>(index)] = QStringList();
        }

        connect(connection_model.get(), &ConnectionModel::NetDisconnecting, this, &BlocklistModel::NetDisconnecting);
    }

    BlocklistModel::~BlocklistModel() = default;

    void BlocklistModel::Load()
    {
        beginResetModel();

        RDP_LOG_INFO("Loading settings group: [{}]", kBlocklistGroup);

        // clear blocklist
        blocklists_.clear();

        // initialize all platform keys
        uint32_t platform_key = static_cast<uint32_t>(Platform::kUnknown) + 1;
        do
        {
            auto platform                        = static_cast<Platform>(platform_key);
            blocklists_[platform]                = QStringList();
            default_blocklist_[platform]         = QStringList();
            removed_default_blocklist_[platform] = QSet<QString>();
            platform_key++;
        } while (platform_key < static_cast<uint32_t>(Platform::kCount));

        // Load blocklist settings from settings.ini if available
        QSettings* settings = settings_manager_->GetSettings();
        Q_ASSERT(settings != nullptr);

        const QStringList groups = settings->childGroups();

        // Check the settings file version

        // If settings version less than 2
        if (const int32_t version = settings->value("version", 1).toInt(); version <= 1 && groups.contains(kBlocklistGroup))
        {
            // Rebuild the blocklist group using the new format which associates the
            // blocked apps with a particular platform

            // Treat all *.exe as Windows platform, all non-suffix as Linux

            settings->beginGroup(kBlocklistGroup);

            const int size = settings->beginReadArray("applications");
            for (int j = 0; j < size; j++)
            {
                settings->setArrayIndex(j);

                QString name = settings->value("name").toString();

                QFileInfo file_info(name);
                if (const QString suffix = file_info.suffix(); suffix == "exe")
                {
                    blocklists_[Platform::kWindows].append(name);
                }
                else
                {
                    blocklists_[Platform::kLinux].append(name);
                }
            }
            settings->endArray();

            settings->endGroup();

            // Now, rewrite the file using the new per-platform blocklist
            settings->beginGroup(kBlocklistGroup);
            for (uint32_t index = static_cast<uint32_t>(Platform::kUnknown) + 1; index < static_cast<uint32_t>(Platform::kCount); index++)
            {
                auto               platform = static_cast<Platform>(index);
                const QStringList& list     = blocklists_[platform];
                const QString      group    = GroupNameFromPlatform(platform);

                settings->beginGroup(group);
                settings->beginWriteArray("applications", list.count());
                for (int k = 0; k < list.count(); k++)
                {
                    const QString& name = list.at(k);
                    settings->setArrayIndex(k);
                    settings->setValue("name", name);
                }
                settings->endArray();
                settings->endGroup();
            }
            settings->endGroup();
            // Bump version number to latest
            settings->setValue("version", "2");
            settings->sync();
        }
        else if (groups.contains(kBlocklistGroup))
        {
            settings->beginGroup(kBlocklistGroup);

            for (uint32_t index = static_cast<uint32_t>(Platform::kUnknown) + 1; index < static_cast<uint32_t>(Platform::kCount); index++)
            {
                auto          platform = static_cast<Platform>(index);
                const QString group    = GroupNameFromPlatform(platform);

                settings->beginGroup(group);
                const int size = settings->beginReadArray("applications");
                for (int j = 0; j < size; j++)
                {
                    settings->setArrayIndex(j);
                    QString name = settings->value("name").toString();
                    blocklists_[platform].append(name);
                }
                settings->endArray();
                settings->endGroup();
            }
            settings->endGroup();
        }

        // Read the default blocklist items that the user has ignored
        if (groups.contains(kRemovedDefaultsGroup))
        {
            settings->beginGroup(kRemovedDefaultsGroup);

            for (uint32_t index = static_cast<uint32_t>(Platform::kUnknown) + 1; index < static_cast<uint32_t>(Platform::kCount); index++)
            {
                auto          platform = static_cast<Platform>(index);
                const QString group    = GroupNameFromPlatform(platform);

                settings->beginGroup(group);
                const int size = settings->beginReadArray("applications");
                for (int j = 0; j < size; j++)
                {
                    settings->setArrayIndex(j);
                    QString name = settings->value("name").toString();
                    removed_default_blocklist_[platform].insert(name);
                }
                settings->endArray();
                settings->endGroup();
            }
            settings->endGroup();
        }

        // Add in the default blocked items
        QSettings default_blocklist(":/blocklist.ini", QSettings::Format::IniFormat);

        for (uint32_t index = static_cast<uint32_t>(Platform::kUnknown) + 1; index < static_cast<uint32_t>(Platform::kCount); index++)
        {
            auto                 platform      = static_cast<Platform>(index);
            const QString        group         = GroupNameFromPlatform(platform);
            const QSet<QString>& ignored_items = removed_default_blocklist_[platform];

            default_blocklist.beginGroup(group);
            const int size = default_blocklist.beginReadArray("applications");
            for (int j = 0; j < size; j++)
            {
                default_blocklist.setArrayIndex(j);
                QString name = default_blocklist.value("name").toString();
                default_blocklist_[platform].append(name);

                // Only add in the item if it was not already removed by the users
                // and is not already in the list.
                if (!ignored_items.contains(name) && !blocklists_[platform].contains(name))
                {
                    blocklists_[platform].append(name);
                }
            }
            default_blocklist.endArray();
            default_blocklist.endGroup();
        }

        endResetModel();

        emit Loaded(shared_from_this());
    }

    void BlocklistModel::Save()
    {
        RDP_LOG_INFO("Saving settings group: [{}]", kBlocklistGroup);

        // Save blocklist settings from settings.ini if available
        QSettings* settings = settings_manager_->GetSettings();
        Q_ASSERT(settings != nullptr);

        settings->setValue("version", QVariant::fromValue(3));

        settings->beginGroup(kBlocklistGroup);

        uint32_t index = static_cast<uint32_t>(Platform::kUnknown) + 1;
        for (; index < static_cast<uint32_t>(Platform::kCount); index++)
        {
            auto               platform = static_cast<Platform>(index);
            const QStringList& list     = blocklists_[platform];
            const QString      group    = GroupNameFromPlatform(platform);

            settings->beginGroup(group);
            settings->beginWriteArray("applications", list.count());
            for (int i = 0; i < list.count(); i++)
            {
                const QString& name = list.at(i);
                settings->setArrayIndex(i);
                settings->setValue("name", name);
            }
            settings->endArray();
            settings->endGroup();
        }
        settings->endGroup();

        settings->beginGroup(kRemovedDefaultsGroup);

        // Save the ignored default applications
        index = static_cast<uint32_t>(Platform::kUnknown) + 1;
        for (; index < static_cast<uint32_t>(Platform::kCount); index++)
        {
            auto                 platform = static_cast<Platform>(index);
            const QSet<QString>& items    = removed_default_blocklist_[platform];
            const QString        group    = GroupNameFromPlatform(platform);

            settings->beginGroup(group);
            settings->beginWriteArray("applications", items.count());

            int array_index = 0;
            for (const QString& name : items)
            {
                settings->setArrayIndex(array_index++);
                settings->setValue("name", name);
            }
            settings->endArray();
            settings->endGroup();
        }
        settings->endGroup();
    }

    void BlocklistModel::OnBind()
    {
        emit PlatformChanged(platform_);
    }

    void BlocklistModel::OnSystemInfoModelLoaded(const std::shared_ptr<SystemInfoModel>& model)
    {
        const QString os_name = model->GetOsName();
        if (const QString os_desc = model->GetOsDescription(); os_name.contains(kWindowsIdentifier) || os_desc.contains(kWindowsIdentifier))
        {
            platform_ = Platform::kWindows;
        }
        else if (os_name.contains(kLinuxIdentifier) || os_desc.contains(kLinuxIdentifier))
        {
            platform_ = Platform::kLinux;
        }
        Q_ASSERT(platform_ != Platform::kUnknown);

        Load();
        emit PlatformChanged(platform_);
    }

    void BlocklistModel::NetDisconnecting()
    {
        platform_ = Platform::kUnknown;

        Load();
        emit PlatformChanged(platform_);
    }

    void BlocklistModel::RestoreDefaults()
    {
        beginResetModel();

        uint32_t platform_key = static_cast<uint32_t>(Platform::kUnknown) + 1;
        do
        {
            auto platform                        = static_cast<Platform>(platform_key);
            blocklists_[platform_]               = default_blocklist_[platform_];
            removed_default_blocklist_[platform] = QSet<QString>();
            platform_key++;
        } while (platform_key < static_cast<uint32_t>(Platform::kCount));

        // Write out defaults to settings
        Save();

        endResetModel();
    }

    void BlocklistModel::AddApplication(const QString& name)
    {
        if (platform_ == Platform::kUnknown)
        {
            return;
        }

        const QString trimmed_name = name.trimmed();
        if (trimmed_name.isEmpty())
        {
            return;
        }

        if (Contains(trimmed_name))
        {
            return;
        }

        auto&     items = blocklists_.at(platform_);
        const int size  = static_cast<int>(items.size());

        beginInsertRows({}, size, size);
        items.push_back(trimmed_name);
        endInsertRows();

        emit OnBlocklistItemChanged(trimmed_name);

        Save();
    }

    bool BlocklistModel::Contains(const QString& name) const
    {
        if (platform_ != Platform::kUnknown)
        {
            return Contains(name, platform_);
        }

        for (uint32_t platform = static_cast<uint32_t>(Platform::kUnknown) + 1; platform < static_cast<uint32_t>(Platform::kCount); ++platform)
        {
            if (Contains(name, static_cast<Platform>(platform)))
            {
                return true;
            }
        }

        return false;
    }

    bool BlocklistModel::Contains(const QString& name, const Platform platform) const

    {
        const auto iterator = blocklists_.find(platform);
        if (iterator == blocklists_.end())
        {
            return false;
        }

        for (const QStringList& blocklist = iterator->second; const QString& item : blocklist)
        {
            QRegularExpression regex_expr = QRegularExpression::fromWildcard(item);

            // If the regex isn't valid, we don't want to ignore the item
            if (!regex_expr.isValid())
            {
                if (name == item)
                {
                    return true;
                }

                continue;
            }

            if (regex_expr.match(name).hasMatch())
            {
                return true;
            }
        }

        return false;
    }

    QVariant BlocklistModel::data(const QModelIndex& index, const int role) const
    {
        const int row = index.row();
        if (platform_ != Platform::kUnknown)
        {
            if (role == Qt::DisplayRole || role == Qt::EditRole)
            {
                return blocklists_.at(platform_).at(row);
            }
        }

        return {};
    }

    Qt::ItemFlags BlocklistModel::flags(const QModelIndex& index) const
    {
        return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
    }

    int BlocklistModel::rowCount(const QModelIndex& parent) const
    {
        Q_UNUSED(parent)

        if (platform_ != Platform::kUnknown)
        {
            return blocklists_.at(platform_).count();
        }
        return 0;
    }

    bool BlocklistModel::setData(const QModelIndex& index, const QVariant& value, const int role)
    {
        const int row = index.row();
        if (role == Qt::EditRole)
        {
            const QString name         = value.toString();
            const QString trimmed_name = name.trimmed();
            if (trimmed_name.isEmpty())
            {
                return false;
            }

            if (Contains(trimmed_name))
            {
                return false;
            }

            blocklists_.at(platform_)[row] = trimmed_name;
            Save();

            emit dataChanged(index, index);
            emit OnBlocklistItemChanged(trimmed_name);

            return true;
        }

        return false;
    }

    bool BlocklistModel::removeRows(const int row, const int count, const QModelIndex& parent)
    {
        // Only remove if row is specified
        if (row >= 0)
        {
            const int last = row + (count - 1);
            beginRemoveRows(parent, row, last);

            // Remove rows from blocklist
            for (int i = row; i <= last; i++)
            {
                QStringList& blocklist = blocklists_.at(platform_);
                QString      name      = blocklist[row];
                blocklist.removeAt(row);

                if (default_blocklist_.at(platform_).contains(name))
                {
                    removed_default_blocklist_.at(platform_).insert(name);
                }
            }

            endRemoveRows();

            // Save immediately
            Save();

            return true;
        }

        return false;
    }

    bool BlocklistModel::insertRows(const int row, const int count, const QModelIndex& parent)
    {
        const int end = row + count;
        beginInsertRows(parent, row, end);

        for (int i = row; i < end; i++)
        {
            blocklists_.at(platform_).insert(i, "");
        }

        endInsertRows();

        return true;
    }

}  // namespace rdp
