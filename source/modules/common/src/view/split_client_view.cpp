// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for split client view.

#include "common/inc/view/split_client_view.h"
#include <QFileDialog>

#include <common/inc/file_client_view.h>
#include <common/inc/model/utility/userdata_view_model.h>

#include "definitions.h"
#include "ui_split_client_view.h"

SplitClientView::SplitClientView(const QString& module_name, QWidget* parent)
    : QWidget(parent)
    , base_ui_(new Ui::SplitClientView())
    , module_name_(module_name)
{
    base_ui_->setupUi(this);

    base_ui_->scroll_area_contents->layout()->setAlignment(Qt::AlignTop);

    base_ui_->status_label->setTextFormat(static_cast<Qt::TextFormat>(base_ui_->status_label->textFormat() | Qt::RichText));
    base_ui_->connected_process_label->setTextFormat(static_cast<Qt::TextFormat>(base_ui_->connected_process_label->textFormat() | Qt::RichText));

    base_ui_->output_path_line_edit->SetMinimumText(R"(C:\Users\ACoolUser\Documents\rra_scenes\SomeCoolApplication)");
    base_ui_->output_path_label->setText(QString("%1 output path").arg(module_name));
    base_ui_->output_path_edit_accessories->hide();

    connect(base_ui_->output_path_browse, &QPushButton::pressed, this, &SplitClientFileView::OnBrowseDumpOutputDirectory);
    connect(base_ui_->output_path_line_edit, &FocusNotificationLineEdit::FocusIn, this, &SplitClientFileView::OutputEditFocusIn);
    connect(base_ui_->output_path_line_edit, &FocusNotificationLineEdit::FocusOut, this, &SplitClientFileView::OutputEditFocusOut);
    connect(base_ui_->output_path_line_edit, &QLineEdit::editingFinished, this, &SplitClientFileView::OutputEditingFinished);

    UpdateUnsupportedDescription("");
}

SplitClientView::~SplitClientView() = default;

void SplitClientView::SetUserdataViewModel(const std::shared_ptr<UserdataViewModel>& userdata_view_model)
{
    userdata_view_model_ = userdata_view_model;

    // Connect the userdata view model's output path changed signal to this view's slot
    if (userdata_view_model_ != nullptr)
    {
        connect(userdata_view_model_.get(), &UserdataViewModel::OnOutputPathChanged, this, &SplitClientView::OnUserdataOutputPathChanged);
    }
}

void SplitClientView::AddRightColumnWidget(QWidget* widget)
{
    base_ui_->recent_trace_column_layout->addWidget(widget);
}

bool SplitClientView::HasContentWidget(QWidget* widget)
{
    return base_ui_->scroll_area_layout->indexOf(widget) >= 0;
}

void SplitClientView::AddContentWidget(QWidget* widget)
{
    if (HasContentWidget(widget))
    {
        return;
    }

    base_ui_->scroll_area_layout->addWidget(widget);
}

void SplitClientView::InsertContentWidget(QWidget* widget, int index)
{
    if (HasContentWidget(widget))
    {
        return;
    }

    base_ui_->scroll_area_layout->insertWidget(index, widget);
}

void SplitClientView::RemoveContentWidget(QWidget* widget)
{
    if (!HasContentWidget(widget))
    {
        return;
    }

    base_ui_->scroll_area_layout->removeWidget(widget);
}

void SplitClientView::SetStatus(const StatusInfo& status)
{
    const QString rich_text = QString("<b>Status: <span style=\"color:%1;\">%2</span></b>").arg(status.color.name(), status.text);
    base_ui_->status_label->setText(rich_text);
}

void SplitClientView::SetConnectedProcessText(const QString& connected_process_text)
{
    if (connected_process_text.isEmpty())
    {
        base_ui_->status_layout->removeWidget(base_ui_->connected_process_label);
        base_ui_->connected_process_label->hide();
    }
    else if (base_ui_->connected_process_label->isHidden())
    {
        base_ui_->status_layout->addWidget(base_ui_->connected_process_label);
        base_ui_->connected_process_label->show();
    }

    const QString rich_text = QString("<b>%1</b>").arg(connected_process_text);
    base_ui_->connected_process_label->setText(rich_text);
}

void SplitClientView::UpdateUnsupportedDescription(const QString& description)
{
    const QString title = QString("%2 capture is not currently supported.").arg(module_name_);
    UpdateStatusDescription(title, description);
}

void SplitClientView::UpdateStatusDescription(const QString& title, const QString& description)
{
    SetShowUnsupportedMessage(!description.isEmpty());

    base_ui_->unsupported_text->setText(QString("<html><body style=\"color: %1\"><p>%2</p><p><span "
                                                "style=\"font-weight:400;\">%3</span></p></body></html>")
                                            .arg(QColor(Qt::darkYellow).name(), title, description));
}

void SplitClientView::SetShowUnsupportedMessage(bool show)
{
    base_ui_->unsupported_text->setVisible(show);
}

void SplitClientView::OnOutputPathChanged(const OutputPathInfo& info)
{
    output_path_ = info.expanded_path;
    raw_path_    = info.raw_path;

    HandleOutputPathChanged(info.expanded_path);

    if (!base_ui_->output_path_line_edit->hasFocus())
    {
        base_ui_->output_path_line_edit->setText(info.expanded_path);
    }
}

void SplitClientView::OutputEditFocusIn()
{
    base_ui_->output_path_line_edit->setText(raw_path_);
    base_ui_->output_path_edit_accessories->show();
}

void SplitClientView::OutputEditFocusOut()
{
    base_ui_->output_path_line_edit->setText(output_path_);
    base_ui_->output_path_edit_accessories->hide();
}

void SplitClientView::OnBrowseDumpOutputDirectory()
{
    // Fill the dialog with the existing trace output path.
    const QString& output_path = base_ui_->output_path_line_edit->text();

    // Open a new folder selection dialog so the user can choose a new output
    // directory.
    const QString new_output_path = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(
        this, kBrowseTraceDirectoryCaptionText, output_path, QFileDialog::Option::ShowDirsOnly | QFileDialog::Option::DontResolveSymlinks));

    if (new_output_path.compare(output_path) == 0)
    {
        return;
    }

    // Only update the trace path if the user's chosen path is valid - empty
    // string indicates a cancel operation
    if (new_output_path.isEmpty())
    {
        return;
    }

    userdata_view_model_->OutputPathChanged(new_output_path);
}

void SplitClientView::OutputEditingFinished()
{
    if (userdata_view_model_ != nullptr)
    {
        userdata_view_model_->OutputPathChanged(base_ui_->output_path_line_edit->text());
    }
}

QString SplitClientView::GetCurrentOutputPath() const
{
    return output_path_;
}

QString SplitClientView::GetModuleName() const
{
    return module_name_;
}

SplitClientFileView::SplitClientFileView(const QString& module_name, const QString& file_concept_name, const QString& extension, QWidget* parent)
    : SplitClientView(module_name, parent)
    , file_client_view_(new FileClientView(file_concept_name, extension, this))
{
    AddRightColumnWidget(file_client_view_.get());
}

SplitClientFileView::~SplitClientFileView() = default;

void SplitClientFileView::HandleOutputPathChanged(const QString& new_path)
{
    file_client_view_->SetOutputPath(new_path);
}

void SplitClientFileView::ReloadFiles()
{
    file_client_view_->Load(GetCurrentOutputPath());
}

void SplitClientView::OnUserdataOutputPathChanged(const QString& output_path)
{
    OnOutputPathChanged(OutputPathInfo{.expanded_path = output_path, .raw_path = output_path});
}
