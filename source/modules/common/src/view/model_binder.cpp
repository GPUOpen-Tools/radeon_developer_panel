// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for view-model binding utility.

#include "common/inc/view/model_binder.h"

void ModelBinder::StartBinding()
{
    for (QMetaObject::Connection const& connection : connections_)
    {
        QObject::disconnect(connection);
    }

    connections_.clear();
}
