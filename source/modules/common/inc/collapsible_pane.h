// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Header for a collapsible pane widget.
#ifndef RDP_SOURCE_MODULES_COMMON_INC_COLLAPSIBLE_PANE_H_
#define RDP_SOURCE_MODULES_COMMON_INC_COLLAPSIBLE_PANE_H_

#include <algorithm>
#include <list>
#include <type_traits>

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainterPath>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <qt_common/utils/qt_util.h>

struct CollapsiblePaneStatics
{
    /// @brief Gets the width and height used for buttons.
    /// @return The width / height for buttons.
    static inline int GetCollapsiblePaneButtonSize();

    /// @brief Gets the horizontal margin for the header.
    /// @return The horizontal margin for the header.
    static inline int GetHeaderMargin();

    /// @brief Gets the corner radius of a collapsible pane
    /// @return The corner radius of a collapsible pane.
    static inline int GetCornerRadius();
};

int CollapsiblePaneStatics::GetCollapsiblePaneButtonSize()
{
    QFont font = QApplication::font();
    font.setPointSizeF(10.5);

    return QFontMetrics(font).height();
}

int CollapsiblePaneStatics::GetHeaderMargin()
{
    return QFontMetrics(QApplication::font()).horizontalAdvance("Al");
}

int CollapsiblePaneStatics::GetCornerRadius()
{
    return GetHeaderMargin() / 2;
}

/// @brief The base class for the collapsible pane widget.
///
/// This class exists so that Q_OBJECT can still be used even though the CollapsiblePane class has templates.
class CollapsiblePaneBase : public QFrame
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] parent The parent widget.
    CollapsiblePaneBase(QWidget* parent = nullptr);

    /// @brief Destructor.
    ~CollapsiblePaneBase() override = default;

protected slots:
    /// @brief Requests that the pane be collapsed if it is expanded or requests that it be expanded if it is collapsed.
    void RequestToggle();

    /// @brief Respond to color theme updated
    void OnColorThemeChanged();

protected:
    /// @brief Collapses the pane if it is already expanded, otherwise expands it.
    virtual void Toggle() = 0;

    /// @brief Updates the collapsible pane's chevron
    virtual void UpdateChevron() = 0;

    QString chevron_icon_down_;   ///< expanded chevron icon name
    QString chevron_icon_right_;  ///< collapsed chevron icon name

private:
    /// @brief Updates icon names based on color theme
    /// @param [in] theme The color theme
    void UpdateIconNames(ColorThemeType theme);
};

/// @brief A widget that will emit a signal every time it receives a left mouse press, consuming the mouse event.
class ClickableWidget : public QWidget
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] parent The parent widget.
    ClickableWidget(QWidget* parent = nullptr);

signals:
    /// @brief Emitted when the left mouse button is pressed on this widget.
    void Pressed();

private:
    /// @brief Handles mouse press events, calling Pressed() as needed.
    /// @param [in] event The mouse event.
    void mousePressEvent(QMouseEvent* event) override;
};

/// @brief A pane that can be collapsed and expanded.
///
/// This class uses a template parameter so tha it can allocate the body widget itself. This enables using collapsible widgets inside of
/// Qt UI files. It's recommended to create a using declaration for the collapsible version of the body to be more ergonomic. For example:
///
/// @code
/// class CoolWidget : public QWidget {...}
/// using CollapsibleCoolWidget = CollapsiblePane<CoolWidget>
/// @endcode
///
/// Then in the UI file, you can use:
/// @code
/// <widget class="CollapsibleCoolWidget" name="test_widget" />
/// @endcode
///
/// Instead of:
///
/// @code
/// <widget class="CollapsiblePane&lt;CoolWidget&gt;" name="test_widget" />
/// @endcode
/// @tparam BodyType The type of widget for the body, should have a constructor that that takes a QWidget to be the parent object.
template <typename BodyType>
class CollapsiblePane : public CollapsiblePaneBase
{
public:
    /// @brief Constructor.
    /// @param [in] parent The parent widget.
    explicit CollapsiblePane(QWidget* parent = nullptr);

    /// @brief Destructor.
    ~CollapsiblePane() override = default;

    /// @brief Returns the body for this pane.
    /// @return The body for this pane.
    BodyType* GetBody();

    /// @brief Collapses the pane.
    void Collapse();

    /// Expands the pane.
    void Expand();

protected:
    /// @brief Collapses the pane if it is already expanded, otherwise expands it.
    void Toggle() override;

    /// @brief Updates the chevron icon on the button based on expanded state.
    void UpdateChevron() override;

public:
    /// @brief Sets the title text for this expandable pane.
    /// @param [in] text The title for the expandable pane.
    void SetTitleText(const QString& text);

    /// @brief Provides the size hint for the collapsible widget.
    /// @return The size hint for the collapsible widget.
    [[nodiscard]] QSize sizeHint() const override;

    /// @brief Provides the minimum size hint for the collapsible widget.
    /// @return The minimum size hint for the collapsible widget.
    [[nodiscard]] QSize minimumSizeHint() const override;

private:
    /// @brief Handles resizing the pane by adjusting the mask to round the corners.
    /// @param [in] event The resize event to handle.
    void resizeEvent(QResizeEvent* event) override;

private:
    QLabel*   chevron_label_ = nullptr;  ///< The label used to display the chevron icon.
    QLabel*   title_label_   = nullptr;  ///< The label that displays the title of the pane.
    BodyType* body_          = nullptr;  ///< The body of the widget that is shown when the pane is expanded.

protected:
    QHBoxLayout* header_layout_ = nullptr;  ///< The header layout.
};

template <typename BodyType>
CollapsiblePane<BodyType>::CollapsiblePane(QWidget* parent)
    : CollapsiblePaneBase(parent)
    , body_(new BodyType(this))
{
    static_assert(std::is_base_of<QWidget, BodyType>::value, "BodyType must be a QWidget");

    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    // Using a label with a few spaces is an easy way to display an icon inline with text
    chevron_label_ = new QLabel(this);
    chevron_label_->setText("     ");
    chevron_label_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    UpdateChevron();

    title_label_ = new QLabel(this);
    title_label_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    QFont bold_font = title_label_->font();
    bold_font.setBold(true);
    title_label_->setFont(bold_font);

    header_layout_ = new QHBoxLayout(this);

    const int header_horizontal_margin = CollapsiblePaneStatics::GetHeaderMargin();
    const int header_vertical_margin   = header_horizontal_margin;

    header_layout_->setContentsMargins(header_horizontal_margin, header_vertical_margin, header_horizontal_margin, header_vertical_margin);
    header_layout_->addWidget(chevron_label_);
    header_layout_->addWidget(title_label_);

    ClickableWidget* header = new ClickableWidget(this);
    header->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    header->setLayout(header_layout_);

    QVBoxLayout* vertical_layout = new QVBoxLayout(this);

    const int body_spacing = fontMetrics().height() / 4;
    vertical_layout->setSpacing(body_spacing);
    vertical_layout->setContentsMargins(0, 0, 0, 0);
    vertical_layout->setAlignment(Qt::AlignTop);
    setLayout(vertical_layout);

    vertical_layout->addWidget(header);
    vertical_layout->addWidget(body_);

    connect(header, &ClickableWidget::Pressed, this, &CollapsiblePane::Toggle);
}

template <typename BodyType>
BodyType* CollapsiblePane<BodyType>::GetBody()
{
    return body_;
}

template <typename BodyType>
void CollapsiblePane<BodyType>::Collapse()
{
    body_->hide();
    UpdateChevron();
}

template <typename BodyType>
void CollapsiblePane<BodyType>::Expand()
{
    body_->show();
    UpdateChevron();
}

template <typename BodyType>
void CollapsiblePane<BodyType>::UpdateChevron()
{
    const QString chevron_icon = body_->isHidden() ? chevron_icon_right_ : chevron_icon_down_;
    chevron_label_->setStyleSheet(QString("image: url(:/%1)").arg(chevron_icon));
}

template <typename BodyType>
void CollapsiblePane<BodyType>::Toggle()
{
    if (body_->isHidden())
    {
        Expand();
    }
    else
    {
        Collapse();
    }
}

template <typename BodyType>
void CollapsiblePane<BodyType>::SetTitleText(const QString& text)
{
    title_label_->setText(text);
}

template <typename BodyType>
QSize CollapsiblePane<BodyType>::sizeHint() const
{
    // Ensures that the width of the pane is the same collapsed and expanded.
    // However, when the body is hidden the height should change.
    const QSize super_size = QWidget::sizeHint();
    if (body_->isVisible())
    {
        return super_size;
    }

    return {std::max<int>(super_size.width(), body_->sizeHint().width()), super_size.height()};
}

template <typename BodyType>
QSize CollapsiblePane<BodyType>::minimumSizeHint() const
{
    // Ensures that the width of the pane is the same collapsed and expanded.
    // However, when the body is hidden the height should change.
    const QSize super_size = QWidget::minimumSizeHint();
    if (body_->isVisible())
    {
        return super_size;
    }

    return {std::max<int>(super_size.width(), body_->minimumSizeHint().width()), super_size.height()};
}

template <typename BodyType>
void CollapsiblePane<BodyType>::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    const int corner_radius = CollapsiblePaneStatics::GetCornerRadius();
    const int mask_radius   = corner_radius + 2;

    setStyleSheet(QString("CollapsiblePaneBase { border-radius: %1px; }").arg(corner_radius));

    QRect top_corner_rect = body_->rect();
    top_corner_rect.setHeight(corner_radius);

    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    path.addRoundedRect(body_->rect(), mask_radius, mask_radius);
    path.addRect(top_corner_rect);

    body_->setMask(QRegion(path.simplified().toFillPolygon().toPolygon()));
}

/// @brief Interface for providing buttons to a collapsible pane.
class CollapsiblePaneButtonProvider
{
public:
    /// @brief Creates the buttons and connects any signals to them.
    /// @param [out] buttons The created buttons.
    virtual void CreateButtons(std::list<QAbstractButton*>& buttons) = 0;
};

template <typename BodyType>
class ButtonCollapsiblePane : public CollapsiblePane<BodyType>
{
    static_assert(std::is_base_of<CollapsiblePaneButtonProvider, BodyType>::value);

public:
    /// @brief Constructor.
    /// @param [in] parent The parent widget.
    explicit ButtonCollapsiblePane(QWidget* parent = nullptr);
};

template <typename BodyType>
ButtonCollapsiblePane<BodyType>::ButtonCollapsiblePane(QWidget* parent)
    : CollapsiblePane<BodyType>(parent)
{
    std::list<QAbstractButton*> buttons;
    CollapsiblePane<BodyType>::GetBody()->CreateButtons(buttons);

    for (auto* button : buttons)
    {
        CollapsiblePane<BodyType>::header_layout_->addWidget(button);
    }
}

#endif
