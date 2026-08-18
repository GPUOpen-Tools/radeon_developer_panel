// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Device clocks model implementation.

#include "device_clocks_model.h"

#include <device_clocks.h>

#include "device_clocks_gpu_model.h"

DeviceClocksModel::DeviceClocksModel(const std::shared_ptr<devtrace::DeviceClocksManager>& manager)
    : manager_(manager)
{
    qRegisterMetaType<QVector<std::shared_ptr<DeviceClocksGpuModel>>>();

    devtrace::DeviceClockGpuEvent event;
    event.listener = this;
    event.callback = [](void* userdata, const devtrace::DeviceClockGpuInfo& info) {
        const auto model = static_cast<DeviceClocksModel*>(userdata);

        QVector<std::shared_ptr<DeviceClocksGpuModel>> gpu_models;
        for (auto& gpu : info.gpus)
        {
            gpu_models.push_back(std::make_shared<DeviceClocksGpuModel>(model->manager_, gpu));
        }

        emit model->GpusChanged(gpu_models, info.is_connected);
    };
    manager_->RegisterEvent(event);
}
