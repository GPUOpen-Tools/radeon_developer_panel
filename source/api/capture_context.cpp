// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for capture context.

#include "capture_context.h"

#include <algorithm>
#include <cstring>

#include <chunk_writing.h>
#include <client_connection_manager.h>
#include <dev_trace_common.h>
#include <device_clocks.h>
#include <logging.h>
#include <overlay_manager.h>
#include <system_info_cache.h>
#include <trace_io.h>

#include <rgd_summary_generator.h>
#include <rgd_trace_source.h>
#include <rgp_trace_source.h>
#include <rmv_trace_source.h>
#include <rra_trace_source.h>

#include "api_allocator.h"
#include "api_clocks.h"
#include "api_logger.h"
#include "api_summary_generator.h"
#include "api_tool.h"
#include "capture_params.h"
#include "client_manager.h"
#include "logging_definitions.h"
#include "trace_source_adapter.h"

RdpCaptureContextImpl::RdpCaptureContextImpl(const RdpCaptureContextInitParams& init_params)
    : init_params_(init_params)
    , blocklist_(std::make_unique<ApiBlocklist>())
{
}

RdpCaptureContextImpl::~RdpCaptureContextImpl()
{
    std::lock_guard lock(state_mutex);
    tool_->Disconnect();

    // We destroy everything before the tool is destroyed so everything has a chance to unregister.
    adapters_.clear();
    resolver_.reset();
    client_manager_.reset();
    clocks_.reset();

    tool_->Destroy();
}

RdpCaptureResult RdpCaptureContextImpl::Initialize()
{
    dipper::Container container{};
    resolver_ = container.GetResolver();

    container.RegisterValue<RdpCaptureAppFilter>(init_params_.app_filter);
    container.RegisterValue<RdpCaptureLogCallback>(init_params_.log_callback);

    container.Register<devtrace::Logger, ApiLogger>();
    logger_ = resolver_->Resolve<std::unique_ptr<devtrace::Logger>>();

    tool_ = ApiTool::CreateTool();
    if (tool_ == nullptr)
    {
        logger_->LogError("Failed to initialze driver APIs.", kLoggingInvalidPid, kLoggingInvalidUmdId);
        return kRdpCaptureResultFailure;
    }

    const bool use_remote = (init_params_.remote_connection.hostname != nullptr) && (init_params_.remote_connection.hostname[0] != '\0');

    if (!use_remote)
    {
        if (!tool_->CreateRouter())
        {
            logger_->LogError("Failed to create router.", kLoggingInvalidPid, kLoggingInvalidUmdId);
            return kRdpCaptureResultFailure;
        }
    }

    ResolveDependencies(container);

    if (!InitializeComponents())
    {
        return kRdpCaptureResultFailure;
    }

    const char*    ip_address = use_remote ? init_params_.remote_connection.hostname : nullptr;
    const uint16_t port       = use_remote ? init_params_.remote_connection.port : 0;

    if (!tool_->ConnectToRouter(ip_address, port))
    {
        logger_->LogError("Failed to connect to router.", kLoggingInvalidPid, kLoggingInvalidUmdId);
        return kRdpCaptureResultFailure;
    }

    if (!QuerySystemInfo())
    {
        logger_->LogError("Failed to query system info.", kLoggingInvalidPid, kLoggingInvalidUmdId);
        return kRdpCaptureResultFailure;
    }

    return kRdpCaptureResultSuccess;
}

void RdpCaptureContextImpl::ResolveDependencies(dipper::Container& container)
{
    container.Install(devtrace::GetDefaultDipperContainer());
    container.Install(tool_->GetApiContainer());

    container.Register<devtrace::RgdSummaryGenerator, ApiSummaryGenerator>();

    container.RegisterValue<ApiBlocklist*>(blocklist_.get());
    container.Register<ClientManager, ClientManager>();
    container.Register<ApiClocks, ApiClocks>();

    std::shared_ptr<MemoryReadWriteStreamProvider> stream_provider = std::make_shared<MemoryReadWriteStreamProvider>();
    container.RegisterValue<std::shared_ptr<devtrace::ReadWriteStreamProvider>>(stream_provider);
    container.RegisterValue<std::shared_ptr<devtrace::ReadWriteStreamOpener>>(stream_provider);
    container.RegisterValue<std::shared_ptr<MemoryReadWriteStreamProvider>>(stream_provider);

    client_manager_ = resolver_->Resolve<std::unique_ptr<ClientManager>>();
    sys_info_cache_ = resolver_->Resolve<std::shared_ptr<devtrace::SystemInfoCache>>();
    clocks_         = resolver_->Resolve<std::unique_ptr<ApiClocks>>();

    driver_utils_api_ = resolver_->Resolve<DDDriverUtilsApi*>();

    CreateAdapter<devtrace::RgpTraceSource>(kRdpCaptureFeatureProfiling);
    CreateAdapter<devtrace::RmvTraceSource>(kRdpCaptureFeatureMemoryTrace);
    CreateAdapter<devtrace::RraTraceSource>(kRdpCaptureFeatureRaytracing);
    CreateAdapter<devtrace::RgdTraceSource>(kRdpCaptureFeatureCrashAnalysis);
}

bool RdpCaptureContextImpl::InitializeComponents()
{
    std::shared_ptr<devtrace::OverlayManager> overlay_manager = resolver_->Resolve<std::shared_ptr<devtrace::OverlayManager>>();
    if (!overlay_manager->Initialize())
    {
        logger_->LogError("Failed to initialize overlay manager.", kLoggingInvalidPid, kLoggingInvalidUmdId);
        return false;
    }

    std::shared_ptr<devtrace::DeviceClocksManager> clock_manager = resolver_->Resolve<std::shared_ptr<devtrace::DeviceClocksManager>>();
    if (!clock_manager->Initialize())
    {
        logger_->LogError("Failed to initialize device clocks manager.", kLoggingInvalidPid, kLoggingInvalidUmdId);
        return false;
    }

    for (const auto& pair : adapters_)
    {
        if (!pair.second->Initialize())
        {
            logger_->LogError("Failed to initialize trace source.", kLoggingInvalidPid, kLoggingInvalidUmdId);
            return false;
        }
    }

    return true;
}

bool RdpCaptureContextImpl::QuerySystemInfo()
{
    auto sys_info = sys_info_cache_->GetSystemInfo();

    if (!sys_info.has_value())
    {
        return false;
    }

    sys_info_ = sys_info.value();
    return true;
}

RdpCaptureResult RdpCaptureContextImpl::EnableFeature(const RdpCaptureFeatureEnableParams* feature_params)
{
    if (feature_params == nullptr)
    {
        return kRdpCaptureResultInvalidParams;
    }

    std::lock_guard lock(state_mutex);

    RdpCaptureFeatureEnableParams params = *feature_params;
    if (adapters_.count(params.feature) == 0)
    {
        return kRdpCaptureResultInvalidParams;
    }

    std::unique_lock connection_lock = client_manager_->LockConnections();
    if (client_manager_->GetProcessInfo().process_id != 0 || enabled_features_.count(params.feature) > 0)
    {
        return kRdpCaptureResultNotReady;
    }

    // Crash analysis is not compatible with other features.
    if ((params.feature == kRdpCaptureFeatureCrashAnalysis && !enabled_features_.empty()) || enabled_features_.count(kRdpCaptureFeatureCrashAnalysis) != 0)
    {
        return kRdpCaptureResultNotReady;
    }

    RdpCaptureResult result = EnableAdapter(params);
    if (result != kRdpCaptureResultSuccess)
    {
        return result;
    }

    if (!EnableDriverFeature(params.feature))
    {
        adapters_[params.feature]->SetEnabled(false);
        return kRdpCaptureResultFailure;
    }

    enabled_features_.insert(params.feature);
    return result;
}

RdpCaptureResult RdpCaptureContextImpl::EnableAdapter(const RdpCaptureFeatureEnableParams& params)
{
    auto& adapter = adapters_[params.feature];
    adapter->SetTraceFinishedCallback(params.trace_finished_callback);
    adapter->SetStatusCallback(params.status_callback);
    adapter->SetProgressCallback(params.progress_callback);
    adapter->SetEnabled(true);

    if (params.feature == kRdpCaptureFeatureProfiling)
    {
        return adapter->ConfigureRgp(params.body.profiling);
    }

    if (params.feature == kRdpCaptureFeatureCrashAnalysis)
    {
        return adapter->ConfigureRgd(params.body.crash_analysis);
    }

    if (params.feature == kRdpCaptureFeatureRaytracing)
    {
        return adapter->ConfigureRra(params.body.raytracing);
    }

    return kRdpCaptureResultSuccess;
}

bool RdpCaptureContextImpl::EnableDriverFeature(const RdpCaptureFeature feature)
{
    if (driver_utils_api_ == nullptr)
    {
        return false;
    }

    DD_DRIVER_UTILS_FEATURE driver_feature{};
    const char*             setter_name = nullptr;

    switch (feature)
    {
    case kRdpCaptureFeatureProfiling:
    case kRdpCaptureFeatureRaytracing:
        driver_feature = DD_DRIVER_UTILS_FEATURE_TRACING;
        setter_name    = "RdpCaptureApi";
        break;
    case kRdpCaptureFeatureCrashAnalysis:
        driver_feature = DD_DRIVER_UTILS_FEATURE_CRASH_ANALYSIS;
        setter_name    = "RdpCaptureApi";
        break;
    default:
        return true;
    }

    const DD_RESULT result = driver_utils_api_->SetFeature(
        driver_utils_api_->pInstance, driver_feature, DD_DRIVER_UTILS_FEATURE_FLAG_ENABLE, setter_name, static_cast<uint32_t>(strlen(setter_name)));

    return result == DD_RESULT_SUCCESS;
}

bool RdpCaptureContextImpl::DisableDriverFeature(const RdpCaptureFeature feature)
{
    if (driver_utils_api_ == nullptr)
    {
        return false;
    }

    DD_DRIVER_UTILS_FEATURE driver_feature{};
    const char*             setter_name = nullptr;

    switch (feature)
    {
    case kRdpCaptureFeatureProfiling:
    case kRdpCaptureFeatureRaytracing:
        driver_feature = DD_DRIVER_UTILS_FEATURE_TRACING;
        setter_name    = "RdpCaptureApi";
        break;
    case kRdpCaptureFeatureCrashAnalysis:
        driver_feature = DD_DRIVER_UTILS_FEATURE_CRASH_ANALYSIS;
        setter_name    = "RdpCaptureApi";
        break;
    default:
        return true;
    }

    const DD_RESULT result = driver_utils_api_->SetFeature(
        driver_utils_api_->pInstance, driver_feature, DD_DRIVER_UTILS_FEATURE_FLAG_IGNORE, setter_name, static_cast<uint32_t>(strlen(setter_name)));

    return result == DD_RESULT_SUCCESS;
}

RdpCaptureResult RdpCaptureContextImpl::DisableFeature(RdpCaptureFeature feature)
{
    std::lock_guard lock(state_mutex);

    if (adapters_.count(feature) == 0)
    {
        return kRdpCaptureResultInvalidParams;
    }

    std::unique_lock connection_lock = client_manager_->LockConnections();
    if (client_manager_->GetProcessInfo().process_id != 0 || enabled_features_.count(feature) == 0)
    {
        return kRdpCaptureResultNotReady;
    }

    auto& adapter = adapters_[feature];
    adapter->SetEnabled(false);

    DisableDriverFeature(feature);

    enabled_features_.erase(feature);
    return kRdpCaptureResultSuccess;
}

void RdpCaptureContextImpl::GetProcessInfo(RdpCaptureProcessInfo* out_process_info)
{
    *out_process_info = client_manager_->GetProcessInfo();
}

RdpCaptureFeatureStage RdpCaptureContextImpl::GetFeatureStage(RdpCaptureFeature feature, uint64_t timeout_ms)
{
    std::lock_guard lock(state_mutex);

    if (adapters_.count(feature) == 0)
    {
        return kRdpCaptureFeatureStageUnknown;
    }

    return adapters_[feature]->GetFeatureStage(timeout_ms);
}

void RdpCaptureContextImpl::GetApiConnections(RdpCaptureFeature feature, RdpCaptureApiConnection** connections, uint64_t* num_connections)
{
    if (connections == nullptr || num_connections == nullptr)
    {
        return;
    }

    std::lock_guard lock(state_mutex);

    if (adapters_.count(feature) == 0)
    {
        *connections     = reinterpret_cast<RdpCaptureApiConnection*>(ApiAlloc(0));
        *num_connections = 0;

        return;
    }

    return adapters_[feature]->GetFeatureApiConnections(connections, num_connections);
}

void RdpCaptureContextImpl::SetTraceDesc([[maybe_unused]] RdpCaptureFeature feature, [[maybe_unused]] const char* desc)
{
    // TODO: Implement this
    std::lock_guard lock(state_mutex);
}

RdpCaptureResult RdpCaptureContextImpl::BeginTrace(RdpCaptureApiConnectionId connection, RdpCaptureFeature feature)
{
    std::lock_guard lock(state_mutex);
    if (adapters_.count(feature) == 0)
    {
        return kRdpCaptureResultInvalidParams;
    }

    return adapters_[feature]->BeginTrace(connection);
}

RdpCaptureResult RdpCaptureContextImpl::AbortTrace(RdpCaptureFeature feature)
{
    std::lock_guard lock(state_mutex);
    if (adapters_.count(feature) == 0)
    {
        return kRdpCaptureResultInvalidParams;
    }

    return adapters_[feature]->AbortTrace();
}

RdpCaptureResult RdpCaptureContextImpl::InsertSnapshot(RdpCaptureApiConnectionId connection, const char* snapshot_name)
{
    std::lock_guard lock(state_mutex);
    if (adapters_.count(kRdpCaptureFeatureMemoryTrace) == 0)
    {
        return kRdpCaptureResultInvalidParams;
    }

    return adapters_[kRdpCaptureFeatureMemoryTrace]->InsertSnapshot(connection, snapshot_name);
}

RdpCaptureResult RdpCaptureContextImpl::DumpTrace(RdpCaptureApiConnectionId connection)
{
    std::lock_guard lock(state_mutex);
    if (adapters_.count(kRdpCaptureFeatureMemoryTrace) == 0)
    {
        return kRdpCaptureResultInvalidParams;
    }

    return adapters_[kRdpCaptureFeatureMemoryTrace]->DumpTrace(connection);
}

void RdpCaptureContextImpl::SetProfilingParams(const RdpCaptureProfilingParams* params)
{
    std::lock_guard lock(state_mutex);
    if (adapters_.count(kRdpCaptureFeatureProfiling) == 0)
    {
        return;
    }

    adapters_[kRdpCaptureFeatureProfiling]->SetProfilingParams(params);
}

void RdpCaptureContextImpl::SetRaytracingParams(const RdpCaptureRaytracingParams* params)
{
    std::lock_guard lock(state_mutex);
    if (adapters_.count(kRdpCaptureFeatureRaytracing) == 0)
    {
        return;
    }

    adapters_[kRdpCaptureFeatureRaytracing]->SetRaytracingParams(params);
}

namespace
{
    // Copies a std::string into a fixed-size char buffer, ensuring null-termination.
    template <size_t N>
    void CopyStringField(char (&dst)[N], const std::string& src)
    {
        const size_t copy_len = std::min(src.size(), N - 1);
        std::memcpy(dst, src.data(), copy_len);
        dst[copy_len] = '\0';
    }
}  // namespace

namespace
{
    void PopulateOs(const system_info_utils::SystemInfo& sys_info, RdpCaptureSystemOs& os)
    {
        os = RdpCaptureSystemOs{};
        CopyStringField(os.name, sys_info.os.name);
        CopyStringField(os.description, sys_info.os.desc);
        CopyStringField(os.hostname, sys_info.os.hostname);
        CopyStringField(os.memory.type, sys_info.os.memory.type);
        os.memory.physical    = sys_info.os.memory.physical;
        os.memory.swap        = sys_info.os.memory.swap;
        os.etw_supported      = sys_info.os.config.etw_support_info.is_supported;
        os.etw_has_permission = sys_info.os.config.etw_support_info.has_permission;
        os.etw_needs_script   = sys_info.os.config.etw_support_info.needs_rgp_registry_or_usergroup;
    }

    void PopulateDriverFromInfo(const system_info_utils::DriverInfo& src, RdpCaptureSystemDriver& driver)
    {
        driver = RdpCaptureSystemDriver{};
        CopyStringField(driver.name, src.name);
        CopyStringField(driver.description, src.description);
        CopyStringField(driver.packaging_version, src.packaging_version);
        if (src.packaging_date.has_value())
        {
            CopyStringField(driver.packaging_date, *src.packaging_date);
        }
        CopyStringField(driver.software_version, src.software_version);
        driver.packaging_version_major = src.packaging_version_major;
        driver.packaging_version_minor = src.packaging_version_minor;
    }

    bool DriverInfoIsEmpty(const system_info_utils::DriverInfo& src)
    {
        return src.name.empty() && src.description.empty() && src.packaging_version.empty() && src.software_version.empty() &&
               (!src.packaging_date.has_value() || src.packaging_date->empty());
    }

    bool PopulateDrivers(const system_info_utils::SystemInfo& sys_info, RdpCaptureSystemDriver** out_drivers, uint64_t* out_num_drivers)
    {
        *out_drivers     = nullptr;
        *out_num_drivers = 0;

        // Mirror the GUI's logic: prefer the per-device drivers[] list when present
        // (Windows), otherwise fall back to a single-entry list synthesized from the
        // system-wide driver record (Linux / older payloads).
        const bool     have_per_device = !sys_info.drivers.empty();
        const uint64_t count           = have_per_device ? sys_info.drivers.size() : (DriverInfoIsEmpty(sys_info.driver) ? 0 : 1);
        if (count == 0)
        {
            return true;
        }

        auto* buf = static_cast<RdpCaptureSystemDriver*>(ApiAlloc(sizeof(RdpCaptureSystemDriver) * count));
        if (buf == nullptr)
        {
            return false;
        }
        std::memset(buf, 0, sizeof(RdpCaptureSystemDriver) * count);

        if (have_per_device)
        {
            for (uint64_t i = 0; i < count; ++i)
            {
                PopulateDriverFromInfo(sys_info.drivers[i], buf[i]);
            }
        }
        else
        {
            PopulateDriverFromInfo(sys_info.driver, buf[0]);
        }

        *out_drivers     = buf;
        *out_num_drivers = count;
        return true;
    }

    bool PopulateCpus(const system_info_utils::SystemInfo& sys_info, RdpCaptureSystemCpu** out_cpus, uint64_t* out_num_cpus)
    {
        *out_cpus     = nullptr;
        *out_num_cpus = 0;

        const uint64_t count = sys_info.cpus.size();
        if (count == 0)
        {
            return true;
        }

        auto* cpu_buf = static_cast<RdpCaptureSystemCpu*>(ApiAlloc(sizeof(RdpCaptureSystemCpu) * count));
        if (cpu_buf == nullptr)
        {
            return false;
        }
        std::memset(cpu_buf, 0, sizeof(RdpCaptureSystemCpu) * count);
        for (uint64_t i = 0; i < count; ++i)
        {
            const system_info_utils::CpuInfo& cpu     = sys_info.cpus[i];
            RdpCaptureSystemCpu&              api_cpu = cpu_buf[i];

            CopyStringField(api_cpu.name, cpu.name);
            CopyStringField(api_cpu.architecture, cpu.architecture);
            CopyStringField(api_cpu.vendor_id, cpu.vendor_id);
            CopyStringField(api_cpu.cpu_id, cpu.cpu_id);
            CopyStringField(api_cpu.device_id, cpu.device_id);
            CopyStringField(api_cpu.virtualization, cpu.virtualization);
            api_cpu.num_physical_cores      = cpu.num_physical_cores;
            api_cpu.num_logical_cores       = cpu.num_logical_cores;
            api_cpu.max_clock_speed_mhz     = cpu.max_clock_speed;
            api_cpu.timestamp_clock_freq_hz = cpu.timestamp_clock_frequency;
        }

        *out_cpus     = cpu_buf;
        *out_num_cpus = count;
        return true;
    }

    bool PopulateGpus(const system_info_utils::SystemInfo& sys_info, uint64_t num_drivers, RdpCaptureGpu** out_gpus, uint64_t* out_num_gpus)
    {
        *out_gpus     = nullptr;
        *out_num_gpus = 0;

        const uint64_t count = sys_info.gpus.size();
        if (count == 0)
        {
            return true;
        }

        auto* gpu_buf = static_cast<RdpCaptureGpu*>(ApiAlloc(sizeof(RdpCaptureGpu) * count));
        if (gpu_buf == nullptr)
        {
            return false;
        }
        std::memset(gpu_buf, 0, sizeof(RdpCaptureGpu) * count);
        for (uint64_t i = 0; i < count; ++i)
        {
            const system_info_utils::GpuInfo& gpu     = sys_info.gpus[i];
            RdpCaptureGpu&                    api_gpu = gpu_buf[i];

            std::memcpy(api_gpu.luid, gpu.asic.id_info.luid, sizeof(RdpCaptureGpuLuid));
            CopyStringField(api_gpu.name, gpu.name);
            api_gpu.vendor_id    = gpu.asic.id_info.vendor;
            api_gpu.subsystem_id = gpu.asic.id_info.subsystem;

            api_gpu.asic.engine_clock_min_hz = gpu.asic.engine_clock_hz.min;
            api_gpu.asic.engine_clock_max_hz = gpu.asic.engine_clock_hz.max;
            api_gpu.asic.gpu_counter_freq_hz = gpu.asic.gpu_counter_freq;
            api_gpu.asic.family              = gpu.asic.id_info.family;
            api_gpu.asic.device_id           = gpu.asic.id_info.device;
            api_gpu.asic.revision            = gpu.asic.id_info.revision;
            api_gpu.asic.e_rev               = gpu.asic.id_info.e_rev;

            CopyStringField(api_gpu.memory.type, gpu.memory.type);
            api_gpu.memory.mem_ops_per_clock = gpu.memory.mem_ops_per_clock;
            api_gpu.memory.bus_bit_width     = gpu.memory.bus_bit_width;
            api_gpu.memory.bandwidth         = gpu.memory.bandwidth;
            api_gpu.memory.mem_clock_min_hz  = gpu.memory.mem_clock_hz.min;
            api_gpu.memory.mem_clock_max_hz  = gpu.memory.mem_clock_hz.max;
            api_gpu.memory.num_heaps         = gpu.memory.heaps.size();
            if (api_gpu.memory.num_heaps > 0)
            {
                api_gpu.memory.heaps = static_cast<RdpCaptureGpuMemoryHeap*>(ApiAlloc(sizeof(RdpCaptureGpuMemoryHeap) * api_gpu.memory.num_heaps));
                if (api_gpu.memory.heaps == nullptr)
                {
                    api_gpu.memory.num_heaps = 0;
                }
                else
                {
                    std::memset(api_gpu.memory.heaps, 0, sizeof(RdpCaptureGpuMemoryHeap) * api_gpu.memory.num_heaps);
                    for (uint64_t h = 0; h < api_gpu.memory.num_heaps; ++h)
                    {
                        const system_info_utils::HeapInfo& heap     = gpu.memory.heaps[h];
                        RdpCaptureGpuMemoryHeap&           api_heap = api_gpu.memory.heaps[h];

                        CopyStringField(api_heap.heap_type, heap.heap_type);
                        api_heap.physical_address = heap.phys_addr;
                        api_heap.size             = heap.size;
                    }
                }
            }

            api_gpu.pci.bus       = gpu.pci.bus;
            api_gpu.pci.device    = gpu.pci.device;
            api_gpu.pci.function  = gpu.pci.function;
            api_gpu.pci.packed_id = gpu.pci.id;

            api_gpu.big_sw.major = gpu.big_sw.major;
            api_gpu.big_sw.minor = gpu.big_sw.minor;
            api_gpu.big_sw.misc  = gpu.big_sw.misc;

            // Map this GPU to an entry in the system-wide drivers[] array.
            // - When system_info_utils provided a per-device driver_index, validate
            //   it points into the exported drivers[] array before exposing it.
            // - Otherwise report -1 per the public contract so consumers know no
            //   per-device mapping was available and can fall back to the legacy
            //   system-wide `driver` field if desired.
            int32_t resolved_index = -1;
            if (gpu.driver_index.has_value())
            {
                const uint32_t parsed = gpu.driver_index.value();
                if (parsed < num_drivers)
                {
                    resolved_index = static_cast<int32_t>(parsed);
                }
            }
            api_gpu.driver_index = resolved_index;
        }

        *out_gpus     = gpu_buf;
        *out_num_gpus = count;
        return true;
    }
}  // namespace

RdpCaptureResult RdpCaptureContextImpl::GetSystemInfo(RdpCaptureSystemInfo* out_info)
{
    if (out_info == nullptr)
    {
        return kRdpCaptureResultInvalidParams;
    }

    RdpCaptureSystemInfo info{};
    PopulateOs(sys_info_, info.os);
    PopulateDriverFromInfo(sys_info_.driver, info.driver);

    if (!PopulateDrivers(sys_info_, &info.drivers, &info.num_drivers) || !PopulateCpus(sys_info_, &info.cpus, &info.num_cpus) ||
        !PopulateGpus(sys_info_, info.num_drivers, &info.gpus, &info.num_gpus))
    {
        FreeSystemInfo(&info);
        return kRdpCaptureResultFailure;
    }

    *out_info = info;
    return kRdpCaptureResultSuccess;
}

void RdpCaptureContextImpl::FreeSystemInfo(RdpCaptureSystemInfo* info)
{
    if (info == nullptr)
    {
        return;
    }

    if (info->cpus != nullptr)
    {
        ApiFree(info->cpus);
        info->cpus = nullptr;
    }
    info->num_cpus = 0;

    if (info->drivers != nullptr)
    {
        ApiFree(info->drivers);
        info->drivers = nullptr;
    }
    info->num_drivers = 0;

    if (info->gpus != nullptr)
    {
        for (uint64_t i = 0; i < info->num_gpus; ++i)
        {
            if (info->gpus[i].memory.heaps != nullptr)
            {
                ApiFree(info->gpus[i].memory.heaps);
                info->gpus[i].memory.heaps = nullptr;
            }
        }
        ApiFree(info->gpus);
        info->gpus = nullptr;
    }
    info->num_gpus = 0;
}

void RdpCaptureContextImpl::GetGpuClockModes(uint64_t gpu_index, RdpCaptureGpuClockModeDetails** modes, uint64_t* num_modes)
{
    if (modes == nullptr || num_modes == nullptr)
    {
        return;
    }

    clocks_->GetGpuClockModes(gpu_index, modes, num_modes);
}

RdpCaptureResult RdpCaptureContextImpl::QueryGpuCurrentClockMode(uint64_t gpu_index, RdpCaptureGpuClockMode* mode)
{
    if (mode == nullptr)
    {
        return kRdpCaptureResultInvalidParams;
    }

    return clocks_->QueryGpuCurrentClockMode(gpu_index, mode);
}

RdpCaptureResult RdpCaptureContextImpl::SetCurrentGpuClockMode(uint64_t gpu_index, RdpCaptureGpuClockMode mode)
{
    return clocks_->SetCurrentGpuClockMode(gpu_index, mode);
}

RdpCaptureResult RdpCaptureContextImpl::AddBlocklistEntry(const char* pattern)
{
    return blocklist_->AddEntry(pattern);
}

RdpCaptureResult RdpCaptureContextImpl::RemoveBlocklistEntry(const char* pattern)
{
    return blocklist_->RemoveEntry(pattern);
}

void RdpCaptureContextImpl::ClearBlocklist()
{
    blocklist_->Clear();
}

void RdpCaptureContextImpl::GetBlocklistEntries(char*** entries, uint64_t* num_entries) const
{
    blocklist_->GetEntries(entries, num_entries);
}

RdpCaptureResult RdpCaptureContextImpl::LoadBlocklistFile(const char* file_path)
{
    return blocklist_->LoadFile(file_path);
}

void RdpCaptureContextImpl::SetBlocklistBlockedCallback(void (*callback)(void*, const char*, uint32_t), void* user_data)
{
    blocklist_->SetBlockedCallback(callback, user_data);
}
