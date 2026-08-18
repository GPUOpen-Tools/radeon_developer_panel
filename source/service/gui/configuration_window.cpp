// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Configuration window class implementation

#include "configuration_window.h"
#include "ui_configuration_window.h"

#include <QEvent>
#include <QIntValidator>
#include <QKeyEvent>

#include "common/inc/restore_cursor_position.h"

#include "definitions.h"
#include "settings.h"
#include "version.h"

static constexpr auto kRemoveListenPortLabel = "Remote listen port:";
static constexpr auto kConnectionWarning     = "Failed to open Radeon Developer Service on port %1";
static constexpr int  kPortWarningTimeout    = 4000;

ConfigurationWindow::ConfigurationWindow(Settings* settings, QWidget* parent)
    : QDialog(parent)
    , ui_(new Ui::ConfigurationWindow)
    , settings_(settings)
    , window_icon_(nullptr)
{
    ui_->setupUi(this);
    ui_->copyrightLabel->setText(ui_->copyrightLabel->text().arg(RDP_BUILD_YEAR));

    // Set the window's flags
    setWindowFlags(Qt::WindowCloseButtonHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowStaysOnTopHint);

    // Allow the user to change the listen port initially.
    EnableChangingPort(true);

    ui_->versionLabelData->setText(RDP_VERSION);
    ui_->buildLabelData->setText(QString::number(RDP_BUILD_NUMBER));
    ui_->buildDateLabelData->setText(RDP_BUILD_DATE_STRING);
    ui_->portBindLabel->hide();

    window_icon_ = new QIcon(":/assets/RDS_Icon.png");
    setWindowIcon(*window_icon_);

    ui_->restoreDefaultSettingButton->setDefault(false);
    ui_->restoreDefaultSettingButton->setAutoDefault(false);

    connect(ui_->restoreDefaultSettingButton, &QPushButton::clicked, this, &ConfigurationWindow::OnDefaultRouterPortClicked);
    connect(ui_->listenPortTextbox, &QLineEdit::editingFinished, this, &ConfigurationWindow::OnRouterPortTextChanged);
    ui_->listenPortTextbox->setValidator(new QIntValidator(0, kMaxListenPort));
    ui_->listenPortTextbox->setText(QString::number(settings_->GetListenPort()));
#ifdef Q_OS_WIN
    ui_->listenPortHeader->setText(kRemoveListenPortLabel);
#endif
    qApp->installEventFilter(this);

    connect(&port_warning_timer_, &QTimer::timeout, this, &ConfigurationWindow::OnPortWarningTimeout);
    port_warning_timer_.setInterval(kPortWarningTimeout);
    port_warning_timer_.setSingleShot(true);
}

ConfigurationWindow::~ConfigurationWindow() Q_DECL_NOEXCEPT
{
    ui_.reset();
}

void ConfigurationWindow::EnableChangingPort(bool enabled)
{
    // Hide the port change warning label.
    ui_->portWarningLabel->setVisible(!enabled);
    ui_->listenPortTextbox->setEnabled(enabled);
    ui_->restoreDefaultSettingButton->setEnabled(enabled);
}

void ConfigurationWindow::OnRouterPortUpdated(unsigned int port)
{
    const QString port_string = QString::number(port);
    ui_->listenPortTextbox->setText(port_string);
}

void ConfigurationWindow::OnRouterPortTextChanged()
{
    if (const auto text = ui_->listenPortTextbox->text(); settings_ != nullptr && settings_->GetListenPort() != text.toUInt())
    {
        const RestoreCursorPosition cache_cursor_position(ui_->listenPortTextbox);
        const uint32_t              port = text.toUInt();
        settings_->SetListenPort(port);

        emit RouterEndpointUpdated();
    }
}

bool ConfigurationWindow::eventFilter(QObject* object, QEvent* event)
{
    bool event_handled = false;
    if (event != nullptr)
    {
        if (event->type() == QEvent::KeyPress)
        {
            if (const auto* key_event = dynamic_cast<QKeyEvent*>(event); key_event != nullptr)
            {
                if ((key_event->key() == Qt::Key_Escape))
                {
                    event_handled = true;
                    close();
                }
            }
        }
    }

    if (!event_handled)
    {
        event_handled = QDialog::eventFilter(object, event);
    }

    return event_handled;
}

void ConfigurationWindow::OnConnectFailed(unsigned int port)
{
    ui_->portBindLabel->setText(QString(kConnectionWarning).arg(port));
    ui_->portBindLabel->show();

    connected_after_error_ = false;
    port_error_timed_out_  = false;

    port_warning_timer_.start();
}

void ConfigurationWindow::OnConnectSucceeded()
{
    connected_after_error_ = true;
    if (port_error_timed_out_)
    {
        ui_->portBindLabel->hide();
    }
}

void ConfigurationWindow::OnPortWarningTimeout()
{
    port_error_timed_out_ = true;
    if (connected_after_error_)
    {
        ui_->portBindLabel->hide();
    }
}

void ConfigurationWindow::OnDefaultRouterPortClicked()
{
    const QString port_string = QString::number(kDefaultConnectionPort);
    ui_->listenPortTextbox->setText(port_string);
    OnRouterPortTextChanged();
}
