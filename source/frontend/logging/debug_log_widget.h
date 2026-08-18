// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Display Widget for Log Data class definition
/// @todo Needs refactor

#ifndef RDP_SOURCE_FRONTEND_DEBUG_LOG_WIDGET_H_
#define RDP_SOURCE_FRONTEND_DEBUG_LOG_WIDGET_H_

#include <memory>

#include <QDateTime>
#include <QHash>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QThread>
#include <QWidget>

#include <common/inc/api/dev_tools_logging.h>

#include "available_logging_filters_model.h"

namespace Ui
{
    class DebugLogWidget;
}

namespace rdp
{
    class LoggingModel;

    /// @brief Proxy model used to filter log messages.
    class DebugLogFilterProxyModel : public QSortFilterProxyModel
    {
    public:
        bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

        /// @brief Sets the filters.
        /// @param [in] source The source to filter by or empty for no filter.
        /// @param [in] pid The PID to filter by or kLoggingInvalidPid for no filter.
        /// @param [in] umd_id The UMD Id to filter by or kLoggingInvalidUmdId for no filter.
        void SetFilters(const QString& source, uint32_t pid, DDConnectionId umd_id);

    private:
        QString        source_;                         ///< The source to filter by.
        uint32_t       pid_    = kLoggingInvalidPid;    ///< The PID to filter by.
        DDConnectionId umd_id_ = kLoggingInvalidUmdId;  ///< The UMD Id to filter by.
    };

    /// @brief Widget used to display the "DebugLog" object and its internal models
    class DebugLogWidget : public QWidget
    {
        Q_OBJECT
    public:
        /// @brief Constructor.
        /// @param [in] logging_model The debug log that handles all of the logging.
        /// @param [in] parent The parent widget.
        explicit DebugLogWidget(const std::shared_ptr<LoggingModel>& logging_model, QWidget* parent = nullptr);

        /// @brief Destructor.
        ~DebugLogWidget() override;

    private slots:
        /// @brief Called when user changes one of the log message filters.
        void OnFiltersChanged();

        /// @brief Opens the location of the log file in the OS' file browser.
        void OpenLogFileLocation();

    private:
        /// @brief Custom delegate subclass used to format LogItems with custom font colors
        /// and text depending on source and level properties.
        class LogItemDelegate : public QStyledItemDelegate
        {
        public:
            /// @brief Constructor.
            /// @param [in] parent The parent object.
            explicit LogItemDelegate(QObject* parent = nullptr);

            /// @brief Does custom painting for a log item.
            /// @param [in] painter The painter to use to paint the log item.
            /// @param [in] option The style options to paint the log item with.
            /// @param [in] index The index of the log item to paint.
            void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
        };

    protected:
        std::unique_ptr<Ui::DebugLogWidget> ui_;  ///< The debug log UI.

    private:
        std::shared_ptr<LoggingModel>                       model_;                ///< The logging model.
        std::shared_ptr<AvailableLoggingFiltersPidModel>    pid_filter_model_;     ///< The model that has all of the available PID filters.
        std::shared_ptr<AvailableLoggingFiltersSourceModel> source_filter_model_;  ///< The model that has all of the available source filters.
        std::shared_ptr<AvailableLoggingFiltersUmdIdModel>  umd_id_filter_model_;  ///< The model that has all of the available UMD connection ID filters.

        std::unique_ptr<DebugLogFilterProxyModel> proxy_model_;                                  ///< The proxy model used to filter the log.
        bool                                      updating_umd_filters_ = false;                 ///< Guard to prevent recursion during UMD filter rebuild.
        uint32_t                                  last_selected_pid_    = kLoggingInvalidPid;    ///< Keeps last selected PID for log output.
        DDConnectionId                            last_selected_umd_id_ = kLoggingInvalidUmdId;  ///< Keeps last selected UMD Id for log output.
    };

}  // namespace rdp

#endif
