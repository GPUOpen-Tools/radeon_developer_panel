// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Utility view V2 class definition.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_UTILITY_VIEW_V2_H_
#define RDP_SOURCE_MODULES_COMMON_INC_UTILITY_VIEW_V2_H_

#include <memory>

#include <QWidget>

#include "model/utility/userdata_view_model.h"
#include "view/model_binder.h"

namespace Ui
{
    class UtilityView;
}

/// @brief Base view for building utility views.
///
/// This view is version 2 and leverages a view model.
class UtilityViewV2 : public QWidget
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] parent The parent widget.
    explicit UtilityViewV2(QWidget* parent = nullptr);

    /// @brief Destructor
    ~UtilityViewV2() noexcept override;

    /// @brief Sets the model for this view.
    /// @param [in] view_model The new model to use with this view.
    virtual void SetBaseModel(const std::shared_ptr<UserdataViewModel>& view_model);

protected:
    /// @brief Sets the container widget contents.
    /// @param [in] contents The widget contents.
    void SetContents(QWidget* contents);

    std::shared_ptr<UserdataViewModel> base_view_model_;  ///< The signals and slots for the view model.
    ModelBinder                        model_binder_;     ///< Utility object used to bind to a model.

private:
    std::unique_ptr<Ui::UtilityView> base_ui_;  ///< Base utility view UI.
};

#endif
