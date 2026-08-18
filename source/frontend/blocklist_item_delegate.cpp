// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief BlocklistItemDelegate class implementation.

#include "blocklist_item_delegate.h"

#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <utility>

namespace rdp
{
    // The following character fails validation for process name editing: /
    // Also, space and . are invalid for the first character in the process name string.
    // Note: double backslash escape characters are needed for regex strings.
    static constexpr const char* kFilenameRegex = "[^./\\s][^/]*";

    BlocklistValidator::~BlocklistValidator() = default;

    void BlocklistValidator::OnApplicationModelLoaded(std::shared_ptr<ApplicationModel> application_model)
    {
        application_model_ = std::move(application_model);
    }

    QValidator::State BlocklistValidator::validate(QString& input, int& pos) const
    {
        // Clear out warning
        emit WarnEntryAlive("");

        if (application_model_ != nullptr)
        {
            if (application_model_->IsApplicationAlive(input))
            {
                emit WarnEntryAlive(input);
                return QValidator::State::Invalid;
            }
        }

        return QRegularExpressionValidator::validate(input, pos);
    }

    BlocklistItemDelegate::BlocklistItemDelegate(QObject* parent)
        : QStyledItemDelegate(parent)
    {
        QRegularExpression expression(kFilenameRegex);
        name_validator_.setRegularExpression(expression);
    }

    BlocklistItemDelegate::~BlocklistItemDelegate() = default;

    const BlocklistValidator& BlocklistItemDelegate::GetValidator() const
    {
        return name_validator_;
    }

    void BlocklistItemDelegate::OnApplicationModelLoaded(std::shared_ptr<ApplicationModel> application_model)
    {
        name_validator_.OnApplicationModelLoaded(std::move(application_model));
    }

    QWidget* BlocklistItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        Q_UNUSED(option)
        Q_UNUSED(index)

        auto* line_edit = new QLineEdit(parent);
        line_edit->setValidator(&name_validator_);

        return line_edit;
    }

}  // namespace rdp
