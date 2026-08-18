// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Display Widget for Log Data class implementation

#include "debug_log_widget.h"

#include <cstdint>

#include <QDebug>
#include <QFontDatabase>
#include <QPainter>

#include "available_logging_filters_model.h"
#include "logging_manager.h"

#include "ui_debug_log_widget.h"
#include "util.h"

namespace rdp
{
    bool DebugLogFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
    {
        const QModelIndex source_index = sourceModel()->index(source_row, 0, {});
        if (!source_index.isValid())
        {
            return false;
        }

        if (!source_.isEmpty() && sourceModel()->data(source_index.siblingAtColumn(LoggingModel::Columns::kSource)).toString() != source_)
        {
            return false;
        }

        if (pid_ != kLoggingInvalidPid && sourceModel()->data(source_index.siblingAtColumn(LoggingModel::Columns::kPid)).toUInt() != pid_)
        {
            return false;
        }

        if (umd_id_ != kLoggingInvalidUmdId && sourceModel()->data(source_index.siblingAtColumn(LoggingModel::Columns::kUmdId)).toUInt() != umd_id_)
        {
            return false;
        }

        return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
    }

    void DebugLogFilterProxyModel::SetFilters(const QString& source, uint32_t pid, DDConnectionId umd_id)
    {
        source_ = source;
        pid_    = pid;
        umd_id_ = umd_id;

        invalidateFilter();
    }

    DebugLogWidget::DebugLogWidget(const std::shared_ptr<LoggingModel>& logging_model, QWidget* parent)
        : QWidget(parent)
        , ui_(new Ui::DebugLogWidget)
        , model_(logging_model)
        , pid_filter_model_(model_->GetPidFilterModel())
        , source_filter_model_(model_->GetSourceFilterModel())
        , umd_id_filter_model_(model_->GetUmdIdFilterModel())
        , proxy_model_(new DebugLogFilterProxyModel)
    {
        ui_->setupUi(this);

        Util::SetCommonQTreeViewProperties(ui_->log_tree_view);

        ui_->log_tree_view->setItemDelegate(new LogItemDelegate(ui_->log_tree_view));

        ui_->log_tree_view->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
        ui_->log_tree_view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

        proxy_model_->setSourceModel(model_.get());
        ui_->log_tree_view->setModel(proxy_model_.get());

        ui_->pid_filter_box->setModel(pid_filter_model_.get());
        ui_->source_filter_box->setModel(source_filter_model_.get());
        ui_->umd_id_filter_box->setModel(umd_id_filter_model_.get());
        ui_->enabled_checkbox->hide();

        // Set initial visibility based on current log file path
        ui_->log_file_location->setVisible(!model_->GetLogFilePath().isEmpty());

        // Update visibility when log file path changes
        connect(model_.get(), &LoggingModel::LogFilePathChanged, this, [this](const QString& path) { ui_->log_file_location->setVisible(!path.isEmpty()); });

        connect(ui_->pid_filter_box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DebugLogWidget::OnFiltersChanged);
        connect(ui_->source_filter_box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DebugLogWidget::OnFiltersChanged);

        connect(ui_->umd_id_filter_box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DebugLogWidget::OnFiltersChanged);
        connect(ui_->log_file_location, &QPushButton::pressed, this, &DebugLogWidget::OpenLogFileLocation);

        // Initialize UMD combo enabled state based on default PID selection
        const QModelIndex init_pid_index = pid_filter_model_->index(ui_->pid_filter_box->currentIndex(), 0, {});
        const uint32_t    init_pid       = pid_filter_model_->GetFilterAtIndex(init_pid_index);
        ui_->umd_id_filter_box->setEnabled(init_pid != kLoggingInvalidPid);
    }

    DebugLogWidget::~DebugLogWidget() = default;

    void DebugLogWidget::OnFiltersChanged()
    {
        if (updating_umd_filters_)
            return;  // prevent recursion

        const QModelIndex pid_index    = pid_filter_model_->index(ui_->pid_filter_box->currentIndex(), 0, {});
        const QModelIndex source_index = source_filter_model_->index(ui_->source_filter_box->currentIndex(), 0, {});

        const uint32_t selected_pid = pid_filter_model_->GetFilterAtIndex(pid_index);
        DDConnectionId selected_umd_id;

        // Enable/disable UMD combo based on PID selection (disabled when PID -> All)
        ui_->umd_id_filter_box->setEnabled(selected_pid != kLoggingInvalidPid);
        if (selected_pid == kLoggingInvalidPid)
        {
            // Clear selection to indicate no option when disabled
            ui_->umd_id_filter_box->setCurrentIndex(0);
            // When PID is "All", force UMD ID filter to "All" as well
            selected_umd_id = kLoggingInvalidUmdId;
        }
        else
        {
            // Get the currently selected UMD ID from the combobox
            const QModelIndex umd_id_index = umd_id_filter_model_->index(ui_->umd_id_filter_box->currentIndex(), 0, {});
            selected_umd_id                = umd_id_filter_model_->GetFilterAtIndex(umd_id_index);
        }

        // Rebuild UMD filters when PID actually changes
        if (selected_pid != last_selected_pid_)
        {
            updating_umd_filters_ = true;
            // When PID changes, rebuild the list of available UMD IDs for that PID
            model_->RebuildUmdFilters(selected_pid, kLoggingInvalidUmdId);
            updating_umd_filters_ = false;
            last_selected_pid_    = selected_pid;
            // Reset last selected UMD ID and combobox selection when PID changes
            last_selected_umd_id_ = kLoggingInvalidUmdId;
            ui_->umd_id_filter_box->setCurrentIndex(0);
            // Update selected_umd_id to match the reset combobox
            selected_umd_id = kLoggingInvalidUmdId;
        }

        // Track last selected UMD ID but don't rebuild the filter model
        if (selected_umd_id != last_selected_umd_id_)
        {
            last_selected_umd_id_ = selected_umd_id;
        }

        proxy_model_->SetFilters(source_filter_model_->GetFilterAtIndex(source_index), selected_pid, selected_umd_id);
    }

    void DebugLogWidget::OpenLogFileLocation()
    {
        Util::BrowseToFile(model_->GetLogFilePath());
    }

    DebugLogWidget::LogItemDelegate::LogItemDelegate(QObject* parent)
        : QStyledItemDelegate(parent)
    {
    }

    void DebugLogWidget::LogItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        // Save the current painter state, so we don't pollute it
        painter->save();

        QPalette application_palette = QApplication::palette();

        QStyleOptionViewItem option_view_item(option);
        option_view_item.features |= QStyleOptionViewItem::HasDisplay;
        option_view_item.text    = index.data(Qt::DisplayRole).toString();
        option_view_item.palette = application_palette;

        const LogLevel log_level = static_cast<LogLevel>(index.data(Qt::UserRole).toInt());
        QColor         color     = application_palette.text().color();

        switch (log_level)
        {
        case LogLevel::kWarning:
            color = Qt::darkYellow;
            break;
        case LogLevel::kError:
            color = Qt::red;
            break;
        default:
            break;
        }

        option_view_item.palette.setColor(QPalette::Text, color);
        option_view_item.palette.setColor(QPalette::HighlightedText, color);

        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &option_view_item, painter);
        painter->restore();
    }
}  // namespace rdp
