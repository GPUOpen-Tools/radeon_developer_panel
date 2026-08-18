// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP system information widget class definition.

#ifndef RDP_SOURCE_FRONTEND_SYSTEM_INFO_WIDGET_H_
#define RDP_SOURCE_FRONTEND_SYSTEM_INFO_WIDGET_H_

#include <memory>

#include <QWidget>

#include "models/system_info_model.h"

namespace Ui
{
    class SystemInfoWidget;
}

namespace rdp
{
    /// @brief Widget that displays the system information.
    class SystemInfoWidget : public QWidget
    {
        Q_OBJECT

    public:
        /// @brief Constructor.
        /// @param parent The parent widget of this widget;
        explicit SystemInfoWidget(QWidget* parent = nullptr);

        /// @brief Destructor.
        ~SystemInfoWidget() Q_DECL_OVERRIDE;

    public slots:

        /// @brief Called when the system model finishes loading.
        /// @param model The system information model that has loaded.
        void OnSystemInfoModelLoaded(std::shared_ptr<SystemInfoModel> model);

        /// @brief Called when the dump system information button is pressed.
        ///
        /// Opens a file dialog to browse for the folder and then dumps the entire system info JSON
        /// to that directory.
        void DumpInfoPressed();

        /// @brief Called when the validity of the model changes.
        /// @param is_valid true if the data in the model is valid, false otherwise.
        void ModelValidityChanged(bool is_valid);

    private:
        std::unique_ptr<Ui::SystemInfoWidget> ui_;                            ///< Qt ui.
        QWidget*                              system_info_container_;         ///< Container widget.
        QVBoxLayout*                          system_info_container_layout_;  ///< Container layout
        std::shared_ptr<SystemInfoModel>      system_info_model_;             ///< Model that stores the system information.
    };

}  // namespace rdp

#endif
