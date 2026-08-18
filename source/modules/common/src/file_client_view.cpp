// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief File based client view class implementation

#include "file_client_view.h"
#include "ui_file_client_view.h"

#include <unordered_map>

#include <QClipboard>
#include <QFileInfo>
#include <QMenu>

#include <qt_common/custom_widgets/message_overlay.h>
#include <qt_common/utils/qt_util.h>

#include "util.h"

using namespace QtCommon;

FileClientView::FileClientView(const QString& file_concept_name, const QString& extension, QWidget* parent)
    : QWidget(parent)
    , base_ui_(new Ui::FileClientView)
    , no_focus_delegate_(new NoFocusDelegate())
{
    qputenv("QT_FILESYSTEMMODEL_WATCH_FILES", "true");
    recent_trace_model_ = std::make_unique<RecentTraceModel>(extension);
    e_msg_timer_        = std::make_unique<QTimer>(this);
    e_msg_timer_->setInterval(10000);  // in milliseconds
    e_msg_timer_->setSingleShot(true);

    file_concept_name_ = file_concept_name.toLower();
    file_concept_name_.replace(0, 1, file_concept_name_[0].toUpper());
    file_concept_name_lower_ = file_concept_name_.toLower();

    base_ui_->setupUi(this);

    // NOTE: This currently assumes the plural of the concept name is not irregular
    base_ui_->recent_files_header->setText(QString("Recently collected %1s").arg(file_concept_name_lower_));
    base_ui_->error_message->hide();

    // Setup the recent trace view
    QtUtils::ApplyStandardTableStyle(base_ui_->recent_files_view);
    base_ui_->recent_files_view->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
    base_ui_->recent_files_view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    base_ui_->recent_files_view->setSortingEnabled(true);
    base_ui_->recent_files_view->setModel(recent_trace_model_.get());
    base_ui_->recent_files_view->sortByColumn(RecentTraceModelBase::kRecentTraceModelDateColumn, Qt::DescendingOrder);
    base_ui_->recent_files_view->setEditTriggers(QAbstractItemView::EditTrigger::SelectedClicked | QAbstractItemView::EditTrigger::EditKeyPressed);

    connect(base_ui_->recent_files_view, &QTreeView::doubleClicked, this, &FileClientView::OnRecentFileDoubleClicked);
    connect(base_ui_->recent_files_view, &QTreeView::customContextMenuRequested, this, &FileClientView::OnShowRecentFilesContextMenu);
    connect(recent_trace_model_.get(), &RecentTraceModel::FileNameValidationError, this, &FileClientView::OnFileNameValidationError);
    connect(e_msg_timer_.get(), &QTimer::timeout, this, &FileClientView::OnHideErrorMessage);

    base_ui_->recent_files_view->setVerticalScrollMode(QAbstractItemView::ScrollMode::ScrollPerPixel);
    base_ui_->recent_files_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    base_ui_->recent_files_view->setItemDelegate(no_focus_delegate_.get());

    base_ui_->recent_files_view->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
}

FileClientView::~FileClientView()
{
    for (QWidget* content_widget : content_widgets_)
    {
        delete content_widget;
    }
}

void FileClientView::SetOutputPath(const QString& path)
{
    Load(path);
}

void FileClientView::Load(const QString& path)
{
    recent_trace_model_->Load(path);
}

void FileClientView::SetEnableOpenContextOption(bool enable_open)
{
    enable_open_context_menu_ = enable_open;
}

void FileClientView::SetEditDropdownOptionsEnabled(bool edit_dropdown_options_enabled)
{
    edit_dropdown_options_enabled_ = edit_dropdown_options_enabled;
}

void FileClientView::AddCustomDropdownOption(const QString&                             name,
                                             const std::function<bool(const QString&)>& is_applicable,
                                             const std::function<void(const QString&)>& execute,
                                             bool                                       is_multiselect,
                                             bool                                       is_bold,
                                             bool                                       is_at_top)
{
    custom_dropdown_options_.emplace_back();
    CustomDropdownOption& option = custom_dropdown_options_.back();
    option.name                  = name;
    option.is_multiselect        = is_multiselect;
    option.is_applicable         = is_applicable;
    option.execute               = execute;
    option.is_bold               = is_bold;
    option.is_at_top             = is_at_top;
}

void FileClientView::OnRecentFileDoubleClicked(const QModelIndex& index)
{
    if (index.isValid())
    {
        QString path = recent_trace_model_->FilePath(index);
        if (ValidatePath(path))
        {
            emit OpenFile(path);
        }
    }
}

bool FileClientView::ValidatePath(QString path, bool showError) const
{
    bool      success = true;
    QFileInfo fileInfo(path);
    if (!fileInfo.exists())
    {
        if (showError)
        {
            MessageOverlay::WarningAsync(QString("Invalid %1").arg(file_concept_name_lower_),
                                         QString("The specified %1 file no longer exists on disk!\n").arg(file_concept_name_lower_));
        }
        success = false;
    }
    else if (!fileInfo.isReadable())
    {
        if (showError)
        {
            MessageOverlay::CriticalAsync(QString("Failed to open %1").arg(file_concept_name_lower_),
                                          QString("The specified %1 is not a readable file!\n").arg(file_concept_name_lower_));
        }
        success = false;
    }
    return success;
}

void FileClientView::OnShowRecentFilesContextMenu(const QPoint& pos)
{
    // Get the index of the clicked cell.
    const QModelIndex selected_index = base_ui_->recent_files_view->indexAt(pos);

    // Did the user select a valid row in the recent traces list?
    const bool is_valid = selected_index.isValid();

    // Did the user select more than one file?
    const int  num_selected_rows    = base_ui_->recent_files_view->selectionModel()->selectedRows().count();
    const bool is_multiple_selected = num_selected_rows > 1;

    // Create Context Menu. The menu options are only enabled if the user
    // right-clicked a valid row.
    QMenu menu;

    // Regardless of which cell was clicked, if it was valid, get the index of
    // the 0th column instead.
    const int     row           = selected_index.row();
    const QString selected_path = recent_trace_model_->FilePath(selected_index);

    std::unordered_map<QAction*, const CustomDropdownOption*> custom_actions;
    AddCustomDropdownActions(is_valid, is_multiple_selected, menu, selected_path, custom_actions, true);

    // Add the "Open profile" entry.
    QAction* open_in_app_action =
        enable_open_context_menu_ ? menu.addAction(is_multiple_selected ? "Open" : QString("Open %1").arg(file_concept_name_lower_)) : nullptr;

    if (open_in_app_action != nullptr)
    {
        QFont open_font = open_in_app_action->font();
        open_font.setBold(true);

        open_in_app_action->setFont(open_font);
        open_in_app_action->setEnabled(is_valid);
    }

#ifdef WIN32
    static const QString kShowInFilesystem = "Show in Explorer";
#else
    static const QString kShowInFilesystem = "Show in File Browser";
#endif

    // Add the "Show in File Browser" entry.
    QAction* open_in_file_browser_action = menu.addAction(kShowInFilesystem);
    open_in_file_browser_action->setEnabled(is_valid && !is_multiple_selected);

    // Add option to copy the path
    QAction* copy_path_action = menu.addAction("Copy path");
    copy_path_action->setEnabled(is_valid && !is_multiple_selected);

    // Add the "Rename trace file" entry.
    QAction* rename_file_action = menu.addAction("Rename");
    rename_file_action->setEnabled(is_valid && !is_multiple_selected && edit_dropdown_options_enabled_);

    // Add the "Delete trace file" entry.
    QAction* delete_action = menu.addAction(is_multiple_selected ? "Delete" : QString("Delete %1").arg(file_concept_name_lower_));
    delete_action->setEnabled(is_valid && edit_dropdown_options_enabled_);

    // Add custom background test action ONLY if extension is rgp/rmv/rra (single selection)
    QAction* background_test_action = nullptr;
    if (getenv("RDP_ENABLE_RDF_BACKEND_TEST"))
    {
        if (is_valid && !is_multiple_selected)
        {
            QString ext = QFileInfo(selected_path).suffix().toLower();
            if (ext == "rgp" || ext == "rmv" || ext == "rra")
            {
                background_test_action = menu.addAction("Background Test");
                background_test_action->setEnabled(true);
            }
        }
    }

    AddCustomDropdownActions(is_valid, is_multiple_selected, menu, selected_path, custom_actions, false);

    // Execute menu actions
    QAction* action = menu.exec(QCursor::pos());

    // Only open the ContextMenu if the user selected a row that has something in
    // it.
    if (is_valid && (action != nullptr))
    {
        QModelIndexList selected_rows = is_multiple_selected ? base_ui_->recent_files_view->selectionModel()->selectedRows() : QModelIndexList{selected_index};
        QFileInfo       file_info(selected_path);

        if (open_in_app_action != nullptr && action == open_in_app_action)
        {
            if (file_info.exists())
            {
                for (const QModelIndex& index : selected_rows)
                {
                    emit OpenFile(recent_trace_model_->FilePath(index));
                }
            }
        }
        else if (action == open_in_file_browser_action)
        {
            if (file_info.exists())
            {
                Util::BrowseToFile(selected_path);
            }
        }
        else if (action == copy_path_action)
        {
            QApplication::clipboard()->setText(QDir::toNativeSeparators(file_info.absoluteFilePath()));
        }
        else if (action == rename_file_action)
        {
            if (file_info.exists())
            {
                QModelIndex edit_index = selected_index.sibling(row, RecentTraceModelBase::kRecentTraceModelNameColumn);
                if (edit_index.isValid())
                {
                    base_ui_->recent_files_view->edit(edit_index);
                }
            }
        }
        else if (action == delete_action)
        {
            recent_trace_model_->Remove(selected_rows);

            for (const QModelIndex& index : selected_rows)
            {
                emit RemovedFile(recent_trace_model_->FilePath(index));
            }

            base_ui_->recent_files_view->clearSelection();
        }
        else if (background_test_action != nullptr && action == background_test_action)
        {
            // Emit signal so a separate component/model can run the backend test and handle output.
            if (file_info.exists())
            {
                emit BackendTestRequested(selected_path);
            }
        }
        else
        {
            if (!file_info.exists())
            {
                return;
            }

            for (const auto& pair : custom_actions)
            {
                if (action == pair.first)
                {
                    if (pair.second->is_multiselect)
                    {
                        for (const QModelIndex& index : selected_rows)
                        {
                            const QString path = recent_trace_model_->FilePath(index);
                            pair.second->execute(path);
                        }
                    }
                    else
                    {
                        pair.second->execute(selected_path);
                    }
                }
            }
        }
    }
}

void FileClientView::OnFileNameValidationError(const QString& message)
{
    e_msg_timer_->stop();
    base_ui_->error_message->setText(message);
    base_ui_->error_message->setVisible(true);
    e_msg_timer_->start();
}

void FileClientView::OnHideErrorMessage()
{
    base_ui_->error_message->hide();
}

void FileClientView::AddCustomDropdownActions(bool                                                       is_valid,
                                              bool                                                       is_multiple_selected,
                                              QMenu&                                                     menu,
                                              const QString&                                             selected_path,
                                              std::unordered_map<QAction*, const CustomDropdownOption*>& custom_actions,
                                              bool                                                       is_top)
{
    for (const CustomDropdownOption& option : custom_dropdown_options_)
    {
        if (option.is_at_top != is_top || !option.is_applicable(selected_path))
        {
            continue;
        }

        if ((option.is_multiselect && !is_multiple_selected) || (is_multiple_selected && !option.is_multiselect))
        {
            continue;
        }

        QAction* custom_action = menu.addAction(option.name);
        if (option.is_bold)
        {
            QFont bold_font = custom_action->font();
            bold_font.setBold(true);

            custom_action->setFont(bold_font);
        }

        custom_actions.insert(std::make_pair(custom_action, &option));
        custom_action->setEnabled(is_valid);
    }
}
