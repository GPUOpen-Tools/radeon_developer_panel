// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Utility view class definition

#ifndef RDP_SOURCE_MODULES_COMMON_INC_UTILITY_VIEW_H_
#define RDP_SOURCE_MODULES_COMMON_INC_UTILITY_VIEW_H_

#include <functional>
#include <memory>

#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QWidget>

#include <ddModule.h>

#include <MercuryModuleExt.h>

namespace Ui
{
    class UtilityView;
}

using QueryCommonApi = std::function<DDModuleCommonApi()>;

class UtilityView : public QWidget
{
    Q_OBJECT
public:
    /// @brief Constructor
    /// @param [in] create_info The mercury utility view create info
    /// @param [in] query_function The module common API query function
    /// @param [in] node_name The module node name
    /// @param [in] parent The parent widget
    explicit UtilityView(const MercuryUtilityViewCreateInfo* create_info, const QueryCommonApi& query_function, QString node_name, QWidget* parent = nullptr);

    /// @brief Destructor
    ~UtilityView() noexcept override;

    /// @brief Loads the data cache by querying for user data from context
    void Load();

    /// @brief Updates the view.
    /// @param [in] update_info The information to update the view with.
    void Update(const struct MercuryUtilityViewUpdateInfo* update_info);

signals:
    /// @brief Signal to initialize UI with default values
    void InitializeToDefaults();

    /// @brief Signal to initialize UI with values from user data
    void Initialize();

    /// @brief Signal values have changed in data cache
    void ValueChanged();

protected slots:

    void OnValueChanged();

protected:
    QJsonObject data_;   ///< Active configuration data
    QJsonObject cache_;  ///< Cached configuration data

    DDModuleDataContext data_context_;

private:
    /// @brief Handle receiving saved user data nodes
    /// @param [in] user_data The user data supplied to pfnQueryUserdataNode
    /// @param [in] data Data node
    /// @param [in] size The node size
    static void ReceiveUserDataNode(void* user_data, const void* data, size_t size);

    MercuryUtilityViewCreateInfo create_info_;     ///< Utility view creation info
    QueryCommonApi               query_function_;  ///< Common API query function
    QString                      node_name_;       ///< User data node name
};

#endif
