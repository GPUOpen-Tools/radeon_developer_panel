// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Utility view class implementation

#include "utility_view.h"

#include <utility>

#include <QTabWidget>
#include <QVBoxLayout>

#include <MercuryModuleExt.h>
#include <ddModule.h>

UtilityView::UtilityView(const MercuryUtilityViewCreateInfo* create_info, const QueryCommonApi& query_function, QString node_name, QWidget* parent)
    : QWidget(parent)
    , data_context_(create_info->hDataContext)
    , create_info_(*create_info)
    , node_name_(std::move(node_name))
{
    Q_ASSERT(query_function != nullptr);
    query_function_ = query_function;

    connect(this, &UtilityView::ValueChanged, this, &UtilityView::OnValueChanged);
}

UtilityView::~UtilityView() noexcept = default;

void UtilityView::Load()
{
    const DDModuleCommonApi kCommonApi = query_function_();

    DD_RESULT result = kCommonApi.pfnQueryUserdataNode(data_context_, qUtf8Printable(node_name_), this, ReceiveUserDataNode);
    if (result == DD_RESULT_COMMON_DOES_NOT_EXIST)
    {
        // Signal that no user data was available for this module. The inheriting
        // module specific utility view can then respond by initializing the UI with default
        // values
        emit InitializeToDefaults();
    }
    else
    {
        emit Initialize();
    }
}

void UtilityView::Update(const MercuryUtilityViewUpdateInfo* update_info)
{
    data_context_ = update_info->hDataContext;
    Load();
}

void UtilityView::OnValueChanged()
{
    // Write out the JSON to user data node
    QJsonDocument document(data_);
    auto          json_blob = document.toJson();

    const DDModuleCommonApi kCommonApi = query_function_();
    kCommonApi.pfnUpdateUserdataNode(data_context_, qUtf8Printable(node_name_), json_blob.data(), json_blob.size() + 1);

    // Trigger a save to disk from panel side
    if (create_info_.pfnValueChanged != nullptr)
    {
        create_info_.pfnValueChanged(create_info_.pUserData);
    }

    // Update the cache to reflect the recently saved data
    cache_ = data_;
}

void UtilityView::ReceiveUserDataNode(void* user_data, const void* data, size_t size)
{
    auto* view = static_cast<UtilityView*>(user_data);
    Q_ASSERT(view != nullptr);
    if (view != nullptr)
    {
        auto          byte_array = QByteArray((char*)data, static_cast<int>(size - 1));
        QJsonDocument doc        = QJsonDocument::fromJson(byte_array);
        view->data_              = doc.object();
        view->cache_             = view->data_;
    }
}
