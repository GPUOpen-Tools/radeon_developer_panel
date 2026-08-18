// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Connection UI class implementation

#include "connection_widget.h"

#include <cassert>

#include <QMessageBox>
#include <QSettings>

#include <qt_common/custom_widgets/message_overlay.h>

#include "logging/logging_manager.h"
#include "mainwindow.h"
#include "timeout_dialog.h"
#include "ui_connection_widget.h"

static constexpr const char* kEnableAutoConnectSettingKey     = "EnableAutoConnectToLastConnection";
static constexpr const char* kDisableClientTimeoutSettingsKey = "DisableClientTimeout";

namespace rdp
{
    ConnectionWidget::HistoryItem::HistoryItem(const LocalConnectionInfo& info)
        : connection_info(QVariant::fromValue(info))
    {
    }

    ConnectionWidget::HistoryItem::HistoryItem(const RemoteConnectionInfo& info)
        : connection_info(QVariant::fromValue(info))
    {
    }

    ConnectionWidget::ConnectionWidget(std::shared_ptr<SettingsManager>      settings_manager,
                                       std::weak_ptr<ConnectionModel>        connection_model,
                                       const std::shared_ptr<TimeoutDialog>& timeout_dialog,
                                       QWidget*                              parent)
        : QWidget(parent)
        , ui_(new Ui::ConnectionWidget)
        , connection_model_(std::move(connection_model))
        , timeout_dialog_(timeout_dialog)
        , current_connection_type_(ConnectionType::kLocal)
        , last_used_index_(-1)
        , enabled_auto_connect_(true)
        , settings_manager_(std::move(settings_manager))
    {
        ui_->setupUi(this);

        LocalConnectionInfo info = QueryLocalConnectionInfo();
        ui_->historyComboBox->addItem(rdp::LocalConnectionInfo::GetDescription(), QVariant::fromValue(HistoryItem(info)));

        // Internal UI events
        connect(ui_->connectButton, &QAbstractButton::clicked, this, &ConnectionWidget::OnConnectButtonClicked);
        connect(ui_->disconnectButton, &QAbstractButton::clicked, this, &ConnectionWidget::OnDisconnectButtonClicked);
        connect(ui_->historyComboBox, QOverload<int>::of(&QComboBox::activated), this, &ConnectionWidget::OnHistoryItemSelected);

        // Connect our handlers to receive save/load settings events
        connect(settings_manager_.get(), &SettingsManager::SettingsSaved, this, &ConnectionWidget::OnSaveSettings);

        // Connect our internal bool with the checkbox
        connect(ui_->cbAutoConnect, &QAbstractButton::clicked, this, [this](bool checked) { enabled_auto_connect_ = checked; });

        std::shared_ptr<ConnectionModel> connection_model_locked = connection_model_.lock();
        Q_ASSERT(connection_model_locked != nullptr);

        connect(ui_->cbClientTimeout, &QCheckBox::clicked, this, &ConnectionWidget::OnDisableClientTimeoutStateChanged);

        connect(connection_model_locked.get(), &ConnectionModel::NetDisconnecting, this, &ConnectionWidget::NetDisconnecting);
        connect(connection_model_locked.get(), &ConnectionModel::NetDisconnected, this, &ConnectionWidget::NetDisconnected);
        connect(connection_model_locked.get(), &ConnectionModel::NetDisconnectedUnexpectedly, this, &ConnectionWidget::OnNetDisconnectedUnexpectedly);

        connect(connection_model_locked.get(), &ConnectionModel::NetConnecting, this, &ConnectionWidget::NetConnecting);
        connect(connection_model_locked.get(), &ConnectionModel::NetConnected, this, &ConnectionWidget::NetConnected);

        connect(connection_model_locked.get(), &ConnectionModel::RouterCreationFailed, this, &ConnectionWidget::OnRouterCreationFailed);
        connect(connection_model_locked.get(), &ConnectionModel::ExistingLocalRouterFound, this, &ConnectionWidget::OnExistingLocalRouterFound);

        ui_->edit_timeouts_button->hide();

        LoadConnectionHistoryFromSettings();
    }

    ConnectionWidget::~ConnectionWidget()
    {
        ui_.reset();
    }

    LocalConnectionInfo ConnectionWidget::QueryLocalConnectionInfo()
    {
        // In the common case, this text is empty. This means we connect to the default bus.
        LocalConnectionInfo info;

        return info;
    }

    RemoteConnectionInfo ConnectionWidget::QueryRemoteConnectionInfo() const
    {
        const QString hostname = ui_->hostNameEdit->text();
        const QString nickname = ui_->nickNameEdit->text();
        const auto    port     = ui_->portNumberSpinBox->value();

        RemoteConnectionInfo info;
        info.hostname = hostname;
        info.nickname = nickname;
        info.port     = port;

        return info;
    }

    void ConnectionWidget::AttemptAutoConnection()
    {
        if (enabled_auto_connect_ && last_used_index_ >= 0 && ui_->historyComboBox->count() > 0 && !connection_model_.expired())
        {
            auto              connection_model = connection_model_.lock();
            const HistoryItem item             = ui_->historyComboBox->itemData(last_used_index_).value<HistoryItem>();

            if (item.connection_info.canConvert<LocalConnectionInfo>())
            {
                connection_model->ConnectToolLocal();

                current_connection_type_ = ConnectionType::kLocal;
            }
            else if (item.connection_info.canConvert<RemoteConnectionInfo>())
            {
                const auto connection_info = item.connection_info.value<RemoteConnectionInfo>();
                connection_model->ConnectToolRemote(connection_info);

                current_connection_type_ = ConnectionType::kRemote;
            }
            else
            {
                // Bad state - ignore this item and fail to auto-connect
            }
        }
    }

    static const int kDefaultPort = 27300;

    void ConnectionWidget::AttemptNewRemoteConnection(const QString& target)
    {
        if (connection_model_.expired())
        {
            return;
        }

        QString hostname = target;
        int     port     = kDefaultPort;

        QStringList target_parts = target.split(":");
        if (target_parts.length() == 2)
        {
            hostname = target_parts.value(0);
            port     = target_parts.value(1).toInt();
        }

        ui_->hostNameEdit->setText(hostname);
        ui_->portNumberSpinBox->setValue(port);

        RemoteConnectionInfo info;
        info.hostname = hostname;
        info.port     = port;

        if (!ValidateHostName())
        {
            MessageOverlay::CriticalAsync("Invalid Host Name", "Please enter a valid host name!");
        }
        else
        {
            const auto connection_model = connection_model_.lock();
            if (connection_model != nullptr)
            {
                connection_model->ConnectToolRemote(info);
            }

            current_connection_type_ = ConnectionType::kRemote;
        }
    }

    void ConnectionWidget::NetDisconnecting()
    {
        // Disable both buttons in transition states
        ui_->connectButton->setEnabled(false);
        ui_->disconnectButton->setEnabled(false);

        // Update the connection status widget
        ui_->textConnectionStatus->setText("Disconnecting...");

        ui_->cbClientTimeout->setEnabled(false);
    }

    void ConnectionWidget::NetDisconnected()
    {
        // You cannot disconnect with no connection, so disable Disconnect
        ui_->connectButton->setEnabled(true);
        ui_->disconnectButton->setEnabled(false);

        // Update the connection status widget
        ui_->textConnectionStatus->setText("No connection");

        ui_->cbClientTimeout->setEnabled(current_connection_type_ == ConnectionType::kLocal);
    }

    void ConnectionWidget::OnNetDisconnectedUnexpectedly()
    {
        MessageOverlay::CriticalAsync("Server disconnect", "Lost connection to service. Please reconnect.");
    }

    void ConnectionWidget::NetConnecting(const QString& description)
    {
        const QString connecting_to_desc = description.isEmpty() ? "Connected" : QString("Connected to %1").arg(description);

        // Disable both buttons in transition states, to simplify our logic when creating a connection
        // (New connections can take several seconds and still fail)
        // The user should wait for the connection to fail, or just disconnect when it finishes.
        ui_->connectButton->setEnabled(false);
        ui_->disconnectButton->setEnabled(false);

        // Update the connection status widget
        ui_->textConnectionStatus->setText(connecting_to_desc);

        ui_->cbClientTimeout->setEnabled(false);
    }

    void ConnectionWidget::NetConnected(const QString& description)
    {
        const QString connected_to_desc = description.isEmpty() ? "Connected" : QString("Connected to %1").arg(description);

        // You cannot connect twice, so disable Connect
        ui_->connectButton->setEnabled(false);
        ui_->disconnectButton->setEnabled(true);

        // Update the connection status widget
        ui_->textConnectionStatus->setText(connected_to_desc);

        // Only enable the 'disable client' timeout button if the model says that it can be done for the current connection
        const auto connection_model = connection_model_.lock();
        ui_->cbClientTimeout->setEnabled(connection_model != nullptr && connection_model->CanClientTimeoutBeDisabledForCurrentConnection());
    }

    void ConnectionWidget::OnRouterCreationFailed()
    {
        MessageOverlay::CriticalAsync("Error",
                                      "RDS could not be started. Make sure that your driver is installed and up-to-date.",
                                      "",
                                      std::function<void(QDialogButtonBox::StandardButton)>(),
                                      QDialogButtonBox::Ok);
    }

    void ConnectionWidget::OnExistingLocalRouterFound()
    {
        MessageOverlay::QuestionAsync("Existing RDS Connection Found",
                                      "Would you like to continue using the existing RDS connection found on the system?",
                                      "",
                                      [&](QDialogButtonBox::StandardButton button) {
                                          const auto connection_model = connection_model_.lock();
                                          if (connection_model == nullptr)
                                          {
                                              return;
                                          }

                                          if (button == QDialogButtonBox::StandardButton::Yes)
                                          {
                                              connection_model->ConnectToolLocal(true);
                                          }
                                      });
    }

    void ConnectionWidget::OnConnectButtonClicked()
    {
        if (connection_model_.expired())
        {
            return;
        }

        // If there is a valid host name entered
        // use remote
        if (ValidateHostName())
        {
            current_connection_type_ = ConnectionType::kRemote;
        }
        else
        {
            current_connection_type_ = ConnectionType::kLocal;
        }

        auto connection_model = connection_model_.lock();
        switch (current_connection_type_)
        {
        case ConnectionType::kLocal:
        {
            settings_manager_->SaveSettings();

            connection_model->ConnectToolLocal();
        }
        break;

        case ConnectionType::kRemote:
        {
            if (!ValidateHostName())
            {
                MessageOverlay::CriticalAsync("Invalid Host Name", "Please enter a valid host name!");
            }
            else
            {
                const RemoteConnectionInfo connection_info = QueryRemoteConnectionInfo();
                HistoryItem                new_item(connection_info);
                RemoteConnectionInfo       new_item_connection_info = new_item.connection_info.value<RemoteConnectionInfo>();

                // Check if there is an item with a matching hostname and port. Nickname is ignored.
                int index = -1;
                for (int i = 0; i < ui_->historyComboBox->count(); i++)
                {
                    const HistoryItem& item = ui_->historyComboBox->itemData(i).value<HistoryItem>();
                    if (item.GetType() == ConnectionType::kRemote)
                    {
                        RemoteConnectionInfo candidate = item.connection_info.value<RemoteConnectionInfo>();
                        if (candidate.hostname == new_item_connection_info.hostname && candidate.port == new_item_connection_info.port)
                        {
                            index = i;
                            break;
                        }
                    }
                }

                bool block = ui_->historyComboBox->blockSignals(true);
                if (index < 0)
                {
                    // Add new connection item
                    ui_->historyComboBox->addItem(new_item_connection_info.GetDescription(), QVariant::fromValue(new_item));

                    // Change selection in history list
                    ui_->historyComboBox->setCurrentIndex(ui_->historyComboBox->count() - 1);
                }
                else
                {
                    // Change selection in history list
                    ui_->historyComboBox->setCurrentIndex(index);

                    // Update the connection info in case the nickname changed
                    ui_->historyComboBox->setItemText(index, new_item_connection_info.GetDescription());
                    ui_->historyComboBox->setItemData(index, QVariant::fromValue(new_item));
                }
                ui_->historyComboBox->blockSignals(block);

                settings_manager_->SaveSettings();

                connection_model->ConnectToolRemote(connection_info);
            }
        }
        break;

        default:
            Q_ASSERT_X(false, "Connect Button Callback", "Unexpected connection type");
        }
    }

    void ConnectionWidget::OnApplicationModelLoaded(std::shared_ptr<ApplicationModel> model)
    {
        application_model_ = std::move(model);

        connect(application_model_.get(), &ApplicationModel::ApplicationConnected, this, &ConnectionWidget::OnClientConnect);
        connect(application_model_.get(), &ApplicationModel::ApplicationDisconnected, this, &ConnectionWidget::OnClientDisconnect);
    }

    void ConnectionWidget::OnDisconnectButtonClicked()
    {
        const auto connection_model = connection_model_.lock();
        if (connection_model != nullptr)
        {
            connection_model->DisconnectTool();
        }
    }

    void ConnectionWidget::OnDisableClientTimeoutStateChanged(bool checked)
    {
        const auto connection_model = connection_model_.lock();
        if (connection_model != nullptr)
        {
            connection_model->SetDisableClientTimeout(checked);
        }
    }

    void ConnectionWidget::LoadConnectionHistoryFromSettings()
    {
        RDP_LOG_INFO("Loading settings group: [{}]", kSettingsPrefix);

        QSettings* settings = settings_manager_->GetSettings();
        Q_ASSERT(settings != nullptr);

        // Fail out if no settings were found
        if (settings->status() != QSettings::Status::NoError)
        {
            RDP_LOG_ERROR("Failure: Invalid QSettings");
            return;
        }
        else
        {
            // Begin our own settings subgroup associated with this widget
            settings->beginGroup(kSettingsPrefix);

            bool saved = settings->value("saved", false).toBool();

            if (saved)
            {
                // Deserialize the user's connection history

                last_used_index_ = settings->value("lastUsedIndex", 0).toInt();

                ui_->historyComboBox->clear();

                // Determine size of user's history
                const uint32_t history_item_count = static_cast<uint32_t>(settings->value("historySize", 0).toInt());

                // Read in an array of HistoryItems
                for (uint32_t i = 0; i < history_item_count; i++)
                {
                    settings->beginGroup(QString("history/%1/").arg(i));

                    QString type_string = settings->value("type", "").toString();
                    if (type_string == "local")
                    {
                        // An empty privateBusId is allowed and the typical use case
                        LocalConnectionInfo info;
                        ui_->historyComboBox->addItem(rdp::LocalConnectionInfo::GetDescription(), QVariant::fromValue(HistoryItem(info)));
                    }
                    else if (type_string == "remote")
                    {
                        const QString hostname = settings->value("hostname", "").toString();
                        const QString nickname = settings->value("nickname", "").toString();
                        const auto    port     = settings->value("port", 0).value<int>();

                        if ((hostname != "") && (port != 0))
                        {
                            RemoteConnectionInfo info;
                            info.hostname = hostname;
                            info.nickname = nickname;
                            info.port     = port;

                            ui_->historyComboBox->addItem(HistoryItem(info).connection_info.value<RemoteConnectionInfo>().GetDescription(),
                                                          QVariant::fromValue(HistoryItem(info)));
                        }
                    }
                    else
                    {
                        std::string what = QString("Unrecognized connection type: %1").arg(type_string).toStdString();
                        Q_ASSERT_X(type_string.length() == 0, __FUNCTION__, what.c_str());
                    }

                    settings->endGroup();
                }

                // Deserialize enable flags
                if (settings->contains(kEnableAutoConnectSettingKey))
                {
                    enabled_auto_connect_ = settings->value(kEnableAutoConnectSettingKey).toBool();
                }

                bool disable_client_timeout = false;
                if (settings->contains(kDisableClientTimeoutSettingsKey))
                {
                    disable_client_timeout = settings->value(kDisableClientTimeoutSettingsKey).toBool();
                }

                if (!connection_model_.expired())
                {
                    connection_model_.lock()->SetDisableClientTimeout(disable_client_timeout);
                }

                // Update the UI
                ui_->cbAutoConnect->setChecked(enabled_auto_connect_);
                ui_->cbClientTimeout->setChecked(disable_client_timeout);

                // Set the default connection to last used in history list
                ui_->historyComboBox->setCurrentIndex(last_used_index_);
                OnHistoryItemSelected(last_used_index_);
            }
            else
            {
                RDP_LOG_WARN("No settings present!");
            }

            // End our settings subgroup
            settings->endGroup();
        }
    }

    bool ConnectionWidget::OnSaveSettings(QSettings* settings) const
    {
        bool success = true;

        RDP_LOG_INFO("Saving settings group: [{}]", kSettingsPrefix);

        Q_ASSERT(settings != nullptr);

        // Check whether the settings file is valid
        if (settings->status() != QSettings::Status::NoError)
        {
            RDP_LOG_ERROR("Failure: Invalid QSettings");
            success = false;
        }
        else
        {
            // Begin settings group for this widget
            settings->beginGroup(kSettingsPrefix);

            settings->setValue("saved", true);
            settings->setValue("historySize", ui_->historyComboBox->count());
            int last_used = ui_->historyComboBox->currentIndex();
            settings->setValue("lastUsedIndex", last_used >= 0 ? last_used : 0);

            // Write out HistoryItem array
            // ignoring the first item which is always local
            for (int i = 0; i < ui_->historyComboBox->count(); i++)
            {
                settings->beginGroup(QString("history/%1/").arg(i));

                // Cleanup any lingering values at this index
                settings->remove("type");
                settings->remove("hostname");
                settings->remove("nickname");
                settings->remove("port");
                settings->remove("privateBusId");

                const HistoryItem item = ui_->historyComboBox->itemData(i).value<HistoryItem>();
                if (item.GetType() == ConnectionType::kLocal)
                {
                    settings->setValue("type", "local");
                }
                else if (item.GetType() == ConnectionType::kRemote)
                {
                    const auto& connection_info = item.connection_info.value<RemoteConnectionInfo>();

                    settings->setValue("type", "remote");
                    settings->setValue("hostname", connection_info.hostname);
                    settings->setValue("nickname", connection_info.nickname);
                    settings->setValue("port", connection_info.port);
                }
                else
                {
                    assert(false && "Saving Unknown");
                }

                settings->endGroup();
            }

            // Serialize enable flags
            settings->setValue(kEnableAutoConnectSettingKey, enabled_auto_connect_);

            if (!connection_model_.expired())
            {
                settings->setValue(kDisableClientTimeoutSettingsKey, connection_model_.lock()->GetIsClientTimeoutDisabled());
            }

            // End this widget's settings group
            settings->endGroup();
        }

        return success;
    }

    // Slot called when a selection is made in the History ComboBox
    void ConnectionWidget::OnHistoryItemSelected(int index)
    {
        const HistoryItem& item = ui_->historyComboBox->itemData(index).value<HistoryItem>();

        // Automatically populate the form based on the selection
        if (item.GetType() == ConnectionType::kLocal)
        {
            ui_->hostNameEdit->clear();
            ui_->nickNameEdit->clear();

            current_connection_type_ = ConnectionType::kLocal;

            const auto connection_model = connection_model_.lock();
            if (connection_model != nullptr)
            {
                ui_->cbClientTimeout->setEnabled(!connection_model->IsConnected() || connection_model->CanClientTimeoutBeDisabledForCurrentConnection());
            }
        }
        else if (item.GetType() == ConnectionType::kRemote)
        {
            const auto& connection_info = item.connection_info.value<RemoteConnectionInfo>();
            ui_->hostNameEdit->setText(connection_info.hostname);
            ui_->nickNameEdit->setText(connection_info.nickname);
            ui_->portNumberSpinBox->setValue(connection_info.port);
            ui_->cbClientTimeout->setEnabled(false);

            current_connection_type_ = ConnectionType::kRemote;
        }
    }

    void ConnectionWidget::OnClientConnect()
    {
        ui_->cbClientTimeout->setEnabled(false);
    }

    void ConnectionWidget::OnClientDisconnect()
    {
        if (!connection_model_.expired())
        {
            auto       connection_model           = connection_model_.lock();
            const bool can_disable_client_timeout = connection_model->CanClientTimeoutBeDisabledForCurrentConnection();
            ui_->cbClientTimeout->setEnabled(can_disable_client_timeout && !application_model_->HasConnectedClients());
        }
    }

    void ConnectionWidget::ShowTimeoutDialog()
    {
        timeout_dialog_->show();
    }

    // Used to determine whether the host name is valid or not,
    // and error out if not
    bool ConnectionWidget::ValidateHostName() const
    {
        return !ui_->hostNameEdit->text().isEmpty();
    }

}  // namespace rdp
