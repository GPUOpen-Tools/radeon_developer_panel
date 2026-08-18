// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition a list view that will be the exact size of it's contents.

#ifndef RDP_SOURCE_FRONTEND_SIDEBAR_TIGHT_LIST_VIEW_H_
#define RDP_SOURCE_FRONTEND_SIDEBAR_TIGHT_LIST_VIEW_H_

#include <QListView>
#include "view/model_binder.h"

/// @brief A list view that will be the exact size of it's contents to prevent scrolling.
class TightListView : public QListView
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] parent The parent widget.
    explicit TightListView(QWidget* parent = nullptr);

    /// @brief Destructor.
    ~TightListView();

    /// @brief Provides a size hint that will make sure that the list view doesn't scroll.
    /// @return The size hint for this widget.
    QSize sizeHint() const override;

    /// @brief Provides a size hint that will make sure that the list view doesn't scroll.
    /// @return The size hint for this widget.
    QSize minimumSizeHint() const override;

    /// @brief Gets the absolute minimum height for this view.
    /// @return The absolute minimum height for this view.
    virtual int GetMinHeight() const;

    /// @brief Sets the model for this view.
    /// @param [in] model The model for this view.
    void setModel(QAbstractItemModel* model) override;

private slots:
    /// @brief Called when the number of rows changes.
    void RowNumberChanged();

protected slots:
    /// @brief Adjust based on changing DPI scales.
    void ScaleFactorChanged();

private:
    ModelBinder model_binder_;  ///< Object used to manage binding to a model.
};

#endif
