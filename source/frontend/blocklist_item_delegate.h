// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team.
/// @file
/// @brief  BlocklistItemDelegate class definition

#ifndef RDP_SOURCE_FRONTEND_BLOCKLIST_ITEM_DELEGATE_H_
#define RDP_SOURCE_FRONTEND_BLOCKLIST_ITEM_DELEGATE_H_

#include <QAbstractItemModel>
#include <QListWidget>
#include <QRegularExpression>
#include <QStyledItemDelegate>

#include "models/application_model.h"

namespace rdp
{
    /// @brief BlocklistValidator implementing client name validation
    class BlocklistValidator : public QRegularExpressionValidator
    {
        Q_OBJECT
    public:
        /// @brief Destructor
        ~BlocklistValidator() Q_DECL_OVERRIDE;

        /// @brief Overridden QValidator::validate
        /// @param [in] input The new input string
        /// @param [in] pos The pos in string
        /// @return validation state
        QValidator::State validate(QString& input, int& pos) const Q_DECL_OVERRIDE;

    public slots:
        /// @brief Handle response to application model loaded
        /// @param [in] model The application model
        void OnApplicationModelLoaded(std::shared_ptr<ApplicationModel> model);

    signals:
        /// @brief Emitted to warn about a validation failure because the application has an active client.
        /// @param entry The name of the application that has an active client.
        void WarnEntryAlive(const QString& entry) const;

    private:
        std::shared_ptr<ApplicationModel> application_model_;  ///< Application model
    };

    /// @brief BlocklistItemDelegate with client name editing validation
    class BlocklistItemDelegate : public QStyledItemDelegate
    {
        Q_OBJECT
    public:
        /// @brief Constructor
        /// @param [in] parent The parent object
        explicit BlocklistItemDelegate(QObject* parent = nullptr);

        /// @brief Destructor
        ~BlocklistItemDelegate() Q_DECL_OVERRIDE;

        /// @brief QStyledItemDelegate::creatEditor() implementation
        /// @param [in] parent The parent widget
        /// @param [in] option The style option
        /// @param [in] index The index for editor
        /// @return QLineEdit with validator for client names
        QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const Q_DECL_OVERRIDE;

        /// @brief Gets the validator for delegate
        /// @return validator for item delegate
        const BlocklistValidator& GetValidator() const;

    public slots:
        /// @brief Handle response to application model loaded
        /// @param [in] model The application model
        void OnApplicationModelLoaded(std::shared_ptr<ApplicationModel> model);

    private:
        BlocklistValidator name_validator_;  ///< blocklist process name validator
    };

}  // namespace rdp

#endif
