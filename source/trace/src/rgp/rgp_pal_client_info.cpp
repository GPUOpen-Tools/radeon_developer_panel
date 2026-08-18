// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Pal Client Info implementation

#include "rgp_pal_client_info.h"

#include <algorithm>

#include <rapidjson/document.h>

#include "dev_trace_common.h"
#include "json/json_mapper.h"

constexpr const char* kTargetGpuKey       = "target_gpu";
constexpr const char* kGpuNameKey         = "gpu_name";
constexpr const char* kDeviceIdKey        = "device_id";
constexpr const char* kRevisionIdKey      = "revision_id";
constexpr const char* kVendorIdKey        = "vendor_id";
constexpr const char* kIsFinalizedKey     = "is_finalized";
constexpr const char* kQueueCountKey      = "queue_count";
constexpr const char* kFrameCountKey      = "frame_count";
constexpr const char* kAttachedScreensKey = "attached_screens";

system_info_utils::GpuInfo PalClientInfo::GetActiveGpu(const std::vector<system_info_utils::GpuInfo>& candidates) const
{
    if (candidates.empty() || gpus_.empty())
    {
        return {};
    }

    uint32_t                   highest_score = Score(gpus_[0]);
    system_info_utils::GpuInfo best_gpu      = GetCorrespondingGpu(gpus_[0], candidates);

    for (size_t i = 1; i < gpus_.size(); ++i)
    {
        uint32_t                   score             = Score(gpus_[i]);
        system_info_utils::GpuInfo corresponding_gpu = GetCorrespondingGpu(gpus_[i], candidates);

        if (score > highest_score)
        {
            highest_score = score;
            best_gpu      = corresponding_gpu;
        }
        else if (score == highest_score && corresponding_gpu.asic.num_cus > best_gpu.asic.num_cus)
        {
            best_gpu = corresponding_gpu;
        }
    }

    return best_gpu;
}

system_info_utils::GpuInfo PalClientInfo::GetCorrespondingGpu(const TargetGpu& gpu, const std::vector<system_info_utils::GpuInfo>& candidates)
{
    for (const auto& candidate : candidates)
    {
        if (gpu.device_id == candidate.asic.id_info.device && gpu.revision_id == candidate.asic.id_info.revision)
        {
            return candidate;
        }
    }

    DEV_TRACE_ASSERT(false);
    return {};
}

std::shared_ptr<PalClientInfo> PalClientInfo::Query(DDDriverUtilsApi* driver_utils_api, DDConnectionId connection_id)
{
    DEV_TRACE_ASSERT(driver_utils_api != nullptr);

    PalClientInfo pal_client_info{};

    DDByteWriter writer{};
    writer.pfnBegin      = PalClientInfo::QueryInfoBegin;
    writer.pfnWriteBytes = PalClientInfo::QueryInfoWrite;
    writer.pfnEnd        = PalClientInfo::QueryInfoEnd;
    writer.pUserdata     = &pal_client_info;

    DD_RESULT result = driver_utils_api->QueryPalDriverInfo(driver_utils_api->pInstance, connection_id, writer);
    if (result == DD_RESULT_SUCCESS && !pal_client_info.gpus_.empty())
    {
        return std::make_shared<PalClientInfo>(pal_client_info);
    }

    return nullptr;
}

DD_RESULT PalClientInfo::QueryInfoBegin(void* user_data, [[maybe_unused]] const size_t* data_sz)
{
    PalClientInfo* pal_client_info = reinterpret_cast<PalClientInfo*>(user_data);
    DEV_TRACE_ASSERT(pal_client_info != nullptr);

    pal_client_info->data_sz_ = 0;
    pal_client_info->json_str_.clear();

    return DD_RESULT_SUCCESS;
}

DD_RESULT PalClientInfo::QueryInfoWrite(void* user_data, const void* data, size_t data_sz)
{
    PalClientInfo* pal_client_info = reinterpret_cast<PalClientInfo*>(user_data);
    DEV_TRACE_ASSERT(pal_client_info != nullptr);

    pal_client_info->data_sz_ += data_sz;
    std::string this_string(static_cast<const char*>(data), data_sz);
    // Append data to json string
    pal_client_info->json_str_.append(this_string);

    return DD_RESULT_SUCCESS;
}

/// @brief Gets the property from the JSON object with a default object if that key is not present.
/// @tparam [in] T The type to convert the specified JSON property into.
/// @param [in] parent The object to get the property from.
/// @param [in] name The name of the property to get from the parent object.
/// @param [in] fallback The default value to use if the value is not present.
/// @return The value from the JSON object or the default value.
template <typename T>
T Get(const rapidjson::Value& parent, char const* name, const T& fallback)
{
    if (parent.HasMember(name))
    {
        T result{};
        if (devtrace::detail::GetValue(parent[name], result))
        {
            return result;
        }
    }

    return fallback;
}

void PalClientInfo::QueryInfoEnd(void* user_data, [[maybe_unused]] DD_RESULT result)
{
    PalClientInfo* pal_client_info = reinterpret_cast<PalClientInfo*>(user_data);
    DEV_TRACE_ASSERT(pal_client_info != nullptr);

    rapidjson::Document pal_info_json;
    pal_info_json.Parse(pal_client_info->json_str_.c_str(), pal_client_info->json_str_.size());
    if (pal_info_json.HasParseError() || !pal_info_json.IsObject() || !pal_info_json.HasMember(kTargetGpuKey))
    {
        return;
    }

    const rapidjson::Value& target_gpu_array = pal_info_json[kTargetGpuKey];
    if (!target_gpu_array.IsArray())
    {
        return;
    }

    for (rapidjson::SizeType i = 0; i < target_gpu_array.Size(); ++i)
    {
        const rapidjson::Value& gpu_node = target_gpu_array[i];
        if (!gpu_node.IsObject())
        {
            continue;
        }

        TargetGpu target_gpu;
        target_gpu.gpu_name         = Get<std::string>(gpu_node, kGpuNameKey, "");
        target_gpu.device_id        = Get<uint32_t>(gpu_node, kDeviceIdKey, 0);
        target_gpu.vendor_id        = Get<uint32_t>(gpu_node, kVendorIdKey, 0);
        target_gpu.revision_id      = Get<uint32_t>(gpu_node, kRevisionIdKey, 0);
        target_gpu.queue_count      = Get<uint32_t>(gpu_node, kQueueCountKey, 0);
        target_gpu.attached_screens = Get<uint32_t>(gpu_node, kAttachedScreensKey, 0);
        target_gpu.frame_count      = Get<uint32_t>(gpu_node, kFrameCountKey, 0);
        target_gpu.is_finalized     = Get<uint32_t>(gpu_node, kIsFinalizedKey, 0);

        pal_client_info->gpus_.push_back(target_gpu);
    }
}

uint32_t PalClientInfo::Score(const TargetGpu& gpu)
{
    return gpu.is_finalized + gpu.queue_count * 10;
}
