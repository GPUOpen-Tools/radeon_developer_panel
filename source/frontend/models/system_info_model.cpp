// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP System info model class implementation

#ifdef _WIN32
#include <Windows.h>

#define SECURITY_WIN32
#include <security.h>

#endif

#include "system_info_model.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLabel>
#include <QSpacerItem>
#include <QTextStream>

#include <memory>
#include <utility>

#include <dd_api_registry_api.h>
#include <dd_router_utils_api.h>

#include <common/inc/formatting.h>

#include "connection_model.h"
#include "logging/logging_manager.h"

using namespace system_info_utils;

static constexpr uint32_t kVanGoghHandheldDeviceId   = 0x163F;
static constexpr uint32_t kVanGoghHandheldRevisionId = 0xAF;
static constexpr uint32_t kStrixHandheldDeviceId     = 0x150E;
static constexpr uint32_t kStrixHandheldRevisionId1  = 0xC5;
static constexpr uint32_t kStrixHandheldRevisionId2  = 0XC7;

static constexpr std::array<std::pair<uint32_t, uint32_t>, 3> kHandheldDevices = {{{kVanGoghHandheldDeviceId, kVanGoghHandheldRevisionId},
                                                                                   {kStrixHandheldDeviceId, kStrixHandheldRevisionId1},
                                                                                   {kStrixHandheldDeviceId, kStrixHandheldRevisionId2}}};

namespace
{
    QLabel* BuildLabel(const QString& text)
    {
        const auto label = new QLabel(nullptr);
        label->setText(text);

        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return label;
    }
}  // namespace

namespace rdp
{
    /// @brief The name of the file that the system info gets dumped into.
    static constexpr auto kJsonExportFilename = "system_info.json";

    /// @brief The substring to look for to determine if an OS is Windows.
    static constexpr auto kWindowsOsName = "Windows";

    /// @brief The substring to look for to determine if an OS is Linux.
    static constexpr auto kLinuxOsName = "Linux";

    /// @brief Converts a boolean value to a user-readable string.
    /// @param boolean The boolean to convert.
    /// @return A user readable string that communicates the value of the boolean.
    QVariant BooleanToString(const bool boolean)
    {
        return {boolean ? "True" : "False"};
    }

    SystemInfoItem::SystemInfoItem(QString key, QVariant value)
        : key_(std::move(key))
        , value_(std::move(value))
        , parent_(nullptr)
    {
    }

    SystemInfoItem::~SystemInfoItem()
    {
        parent_ = nullptr;
        children_.clear();
    }

    std::shared_ptr<SystemInfoItem> SystemInfoItem::AddChild(const std::shared_ptr<SystemInfoItem>& item)
    {
        children_.push_back(item);
        children_.back()->parent_ = this;

        return children_.back();
    }

    const QString& SystemInfoItem::GetKey() const
    {
        return key_;
    }

    const QVariant& SystemInfoItem::GetValue() const
    {
        return value_;
    }

    const SystemInfoItem* SystemInfoItem::GetParent() const
    {
        return parent_;
    }

    const SystemInfoItem* SystemInfoItem::GetChild(const int row) const
    {
        if (children_.empty())
        {
            return nullptr;
        }

        return children_[row].get();
    }

    int SystemInfoItem::GetChildCount() const
    {
        return static_cast<int>(children_.size());
    }

    int SystemInfoItem::GetRow() const
    {
        // Invalid to call on root
        Q_ASSERT(parent_ != nullptr);

        // Get the row of this item by checking the parent child list for its index
        const auto location =
            std::ranges::find_if(parent_->children_, [this](const std::shared_ptr<SystemInfoItem>& item) { return this->GetKey() == item->GetKey(); });
        const int row = static_cast<int>(std::distance(parent_->children_.begin(), location));
        return row;
    }

    void SystemInfoModel::QuerySystemInfoJsonCallback(void* user_data, const char* text)
    {
        Q_ASSERT(user_data != nullptr);

        auto* system_info_model       = static_cast<SystemInfoModel*>(user_data);
        system_info_model->json_data_ = QString(text);

        system_info_model->json_document_ = QJsonDocument::fromJson(text);
        SystemInfoReader::Parse(text, system_info_model->info_);
        system_info_model->beginResetModel();
        const SystemInfo& info = system_info_model->info_;
        system_info_model->root_.reset(new SystemInfoItem(QString("root"), QVariant()));
        system_info_model->root_->AddChild(std::make_shared<SystemInfoItem>(QString("OS Name"), QVariant(info.os.name.c_str())));
        system_info_model->root_->AddChild(std::make_shared<SystemInfoItem>(QString("OS Description"), QVariant(info.os.desc.c_str())));
        if (info.os.name.find(kLinuxOsName) != std::string::npos || info.os.desc.find(kLinuxOsName) != std::string::npos)
        {
            system_info_model->root_->AddChild(
                std::make_shared<SystemInfoItem>(QString("Power DPM Writable"), BooleanToString(info.os.config.power_dpm_writable)));
        }
        if (info.os.name.find(kWindowsOsName) != std::string::npos)
        {
            system_info_model->root_->AddChild(
                std::make_shared<SystemInfoItem>(QString("ETW Support"), QVariant(info.os.config.etw_support_info.is_supported ? "Enabled" : "Disabled")));
            system_info_model->root_->AddChild(
                std::make_shared<SystemInfoItem>(QString("ETW Has Permission"), BooleanToString(info.os.config.etw_support_info.has_permission)));
            system_info_model->root_->AddChild(
                std::make_shared<SystemInfoItem>(QString("ETW Status Code"), QVariant(static_cast<qint64>(info.os.config.etw_support_info.status_code))));
        }  // Grab all GPU info
        for (size_t i = 0; i < system_info_model->info_.gpus.size(); i++)
        {
            const auto& gpu = system_info_model->info_.gpus[i];

            auto gpu_item = std::make_shared<SystemInfoItem>(QString("GPU %1").arg(i), QString::fromStdString(gpu.name));

            // Build ASIC section
            auto asic_item = std::make_shared<SystemInfoItem>(QString("ASIC"), QVariant());
            {
                asic_item->AddChild(
                    std::make_shared<SystemInfoItem>(QString("Engine Clock Min Hz"), QVariant(static_cast<qint64>(gpu.asic.engine_clock_hz.min))));
                asic_item->AddChild(
                    std::make_shared<SystemInfoItem>(QString("Engine Clock Max Hz"), QVariant(static_cast<qint64>(gpu.asic.engine_clock_hz.max))));
                asic_item->AddChild(std::make_shared<SystemInfoItem>(QString("Counter Frequency"), QVariant(static_cast<qint64>(gpu.asic.gpu_counter_freq))));

                // IDs
                const auto& identifiers = std::make_shared<SystemInfoItem>(QString("Identifiers"), QVariant());
                identifiers->AddChild(std::make_shared<SystemInfoItem>(QString("Device"), QVariant(static_cast<qint32>(gpu.asic.id_info.device))));
                identifiers->AddChild(std::make_shared<SystemInfoItem>(QString("Revision"), QVariant(static_cast<qint32>(gpu.asic.id_info.revision))));
                identifiers->AddChild(std::make_shared<SystemInfoItem>(QString("eRevision"), QVariant(static_cast<qint32>(gpu.asic.id_info.e_rev))));
                identifiers->AddChild(std::make_shared<SystemInfoItem>(QString("Family"), QVariant(static_cast<qint32>(gpu.asic.id_info.family))));
                identifiers->AddChild(std::make_shared<SystemInfoItem>(QString("GFX Engine"), QVariant(static_cast<qint32>(gpu.asic.id_info.gfx_engine))));

                asic_item->AddChild(identifiers);
            }
            gpu_item->AddChild(asic_item);

            // Build Memory section
            auto memory_item = std::make_shared<SystemInfoItem>(QString("Memory"), QVariant());
            {
                memory_item->AddChild(std::make_shared<SystemInfoItem>(QString("Type"), QVariant(QString::fromStdString(gpu.memory.type).toUpper())));
                memory_item->AddChild(std::make_shared<SystemInfoItem>(QString("Bandwidth"), QVariant(static_cast<qint64>(gpu.memory.bandwidth))));
                memory_item->AddChild(std::make_shared<SystemInfoItem>(QString("Bus Bit Width"), QVariant(static_cast<qint32>(gpu.memory.bus_bit_width))));
                memory_item->AddChild(
                    std::make_shared<SystemInfoItem>(QString("Memory Clock Hz Min"), QVariant(static_cast<qint64>(gpu.memory.mem_clock_hz.min))));
                memory_item->AddChild(
                    std::make_shared<SystemInfoItem>(QString("Memory Clock Hz Max"), QVariant(static_cast<qint64>(gpu.memory.mem_clock_hz.max))));
                memory_item->AddChild(
                    std::make_shared<SystemInfoItem>(QString("Memory Ops Per Clock"), QVariant(static_cast<qint32>(gpu.memory.mem_ops_per_clock))));

                if (!gpu.memory.excluded_va_ranges.empty())
                {
                    // Excluded VA Ranges
                    auto ex_va_ranges = std::make_shared<SystemInfoItem>(QString("Excluded VA Ranges"), QVariant());
                    {
                        for (size_t j = 0; j < gpu.memory.excluded_va_ranges.size(); j++)
                        {
                            const auto& [base, size] = gpu.memory.excluded_va_ranges[j];

                            auto range_item = std::make_shared<SystemInfoItem>(QString("Range %1").arg(j), QVariant());
                            range_item->AddChild(std::make_shared<SystemInfoItem>(QString("Base"), QVariant(static_cast<qint64>(base))));
                            range_item->AddChild(std::make_shared<SystemInfoItem>(QString("Size"), QVariant(static_cast<qint64>(size))));

                            ex_va_ranges->AddChild(range_item);
                        }
                    }
                    memory_item->AddChild(ex_va_ranges);
                }

                if (!gpu.memory.heaps.empty())
                {
                    // Heaps
                    auto heaps = std::make_shared<SystemInfoItem>(QString("Heaps"), QVariant());
                    {
                        for (const auto& [heap_type, phys_addr, size] : gpu.memory.heaps)
                        {
                            auto heap_item = std::make_shared<SystemInfoItem>(QString(heap_type.c_str()), QVariant());
                            heap_item->AddChild(std::make_shared<SystemInfoItem>(QString("Physical Address"), QVariant(static_cast<qint64>(phys_addr))));
                            heap_item->AddChild(std::make_shared<SystemInfoItem>(QString("Size"), QVariant(static_cast<qint64>(size))));

                            heaps->AddChild(heap_item);
                        }
                    }
                    memory_item->AddChild(heaps);
                }
            }

            gpu_item->AddChild(memory_item);

            // Add to root
            system_info_model->root_->AddChild(gpu_item);
        }
        system_info_model->endResetModel();
        system_info_model->has_info_ = true;

        emit system_info_model->SystemInfoChanged(system_info_model->json_data_);
    }

    SystemInfoModel::SystemInfoModel(const std::shared_ptr<ConnectionModel>& connection_model, QObject* parent)
        : QAbstractItemModel(parent)
        , connection_model_(connection_model)
    {
        root_.reset(new SystemInfoItem(QString("root"), QVariant()));

        connect(connection_model_.get(), &ConnectionModel::NetDisconnecting, this, &SystemInfoModel::NetBusDisconnect);
        connect(connection_model_.get(), &ConnectionModel::NetConnected, this, &SystemInfoModel::Load);
    }

    SystemInfoModel::~SystemInfoModel() Q_DECL_NOEXCEPT = default;

    void SystemInfoModel::Load()
    {
        has_info_ = false;

        if (!LoadSystemInfoApi())
        {
            RDP_LOG_ERROR("Failed to get system info api");
            emit IsDataValidChanged(false);
            emit SystemInfoFailedToLoad();

            return;
        }

        size_t      required_size;
        std::string json_buffer;

        DD_RESULT result = router_utils_api_->GetSysInfo(router_utils_api_->pInstance, nullptr, &required_size);
        if (result == DD_RESULT_SUCCESS)
        {
            json_buffer.resize(required_size);
            result = router_utils_api_->GetSysInfo(router_utils_api_->pInstance, json_buffer.data(), &required_size);
        }

        if (result != DD_RESULT_SUCCESS)
        {
            RDP_LOG_ERROR("Failed to load system info");
            emit IsDataValidChanged(false);
            emit SystemInfoFailedToLoad();

            return;
        }

        has_info_ = true;

        QuerySystemInfoJsonCallback(this, json_buffer.c_str());

        emit Loaded(shared_from_this());
        emit IsDataValidChanged(has_info_);

        PostSystemInfoLoad();
    }

    bool SystemInfoModel::LoadSystemInfoApi()
    {
        if (router_utils_api_ != nullptr)
        {
            return true;
        }

        const DDApiRegistry* api_registry = connection_model_->GetApiRegistry();
        if (api_registry == nullptr)
        {
            return false;
        }

        const DD_RESULT api_get_result =
            api_registry->Get(api_registry->pInstance,
                              DD_ROUTER_UTILS_API_NAME,
                              DDVersion{DD_ROUTER_UTILS_API_VERSION_MAJOR, DD_ROUTER_UTILS_API_VERSION_MINOR, DD_ROUTER_UTILS_API_VERSION_PATCH},
                              reinterpret_cast<void**>(&router_utils_api_));

        return api_get_result == DD_RESULT_SUCCESS;
    }

    void SystemInfoModel::PostSystemInfoLoad()
    {
        if (IsWindows())
        {
            if (GetEtwIsSupported())
            {
                if (const ulong etw_status_code = GetEtwStatusCode(); etw_status_code != 0)
                {
                    emit EtwStatusCodeNonzero(GetEtwHasPermission());
                }
            }

            if (GetAddUserToGroupScriptNeeded())
            {
                emit AddUserToGroupBatNeeded();
            }

            // Check if system is a VanGogh/Strix handheld device
            if (IsVanGoghOrStrixHandheld())
            {
                // If we don't have packaging date info, show prompt anyway
                if (const auto packaging_date = GetDriverPackagingDate(); !packaging_date.has_value())
                {
                    emit UnsupportedHandheldDriver();
                }
                else
                {
                    // Require at least 25.10 or newer
                    const bool   is_older_than_2510 = devtrace::IsDriverTooOld(info_.driver, 25, 10);
                    const QDate& date               = packaging_date.value();
                    // Check to make sure driver is newer than baseline date
                    if (const auto baseline = QDate(2025, 07, 28); is_older_than_2510 || date < baseline)
                    {
                        emit UnsupportedHandheldDriver();
                    }
                }
            }
        }
    }

    bool SystemInfoModel::IsValid() const
    {
        return has_info_;
    }

    bool SystemInfoModel::IsWindows() const
    {
        const QString os_name = GetOsName();
        const QString os_desc = GetOsDescription();

        return os_name.contains("Windows") || os_desc.contains("Windows");
    }

    bool SystemInfoModel::IsLinux() const
    {
        const QString os_name = GetOsName();
        const QString os_desc = GetOsDescription();

        return os_name.contains("Linux") || os_desc.contains("Linux");
    }

    QString SystemInfoModel::GetOsName() const
    {
        return QString::fromStdString(info_.os.name);
    }

    QString SystemInfoModel::GetOsDescription() const
    {
        return QString::fromStdString(info_.os.desc);
    }

    bool SystemInfoModel::GetEtwIsSupported() const
    {
        return info_.os.config.etw_support_info.is_supported;
    }

    bool SystemInfoModel::GetAddUserToGroupScriptNeeded() const
    {
        return info_.os.config.etw_support_info.needs_rgp_registry_or_usergroup;
    }

    bool SystemInfoModel::GetEtwHasPermission() const
    {
        return info_.os.config.etw_support_info.has_permission;
    }

    ulong SystemInfoModel::GetEtwStatusCode() const
    {
        return info_.os.config.etw_support_info.status_code;
    }

    bool SystemInfoModel::IsVanGoghOrStrixHandheld() const
    {
        if (info_.gpus.empty())
            return false;

        const auto& gpu         = info_.gpus.front();
        const auto  device_id   = gpu.asic.id_info.device;
        const auto  revision_id = gpu.asic.id_info.revision;

        for (const auto& [hh_device_id, hh_revision_id] : kHandheldDevices)
        {
            if (device_id == hh_device_id && revision_id == hh_revision_id)
            {
                return true;
            }
        }

        return false;
    }

    std::optional<QDate> SystemInfoModel::GetDriverPackagingDate() const
    {
        if (!info_.driver.packaging_date || info_.driver.packaging_date->empty())
        {
            return std::nullopt;
        }

        const std::string_view date_string  = info_.driver.packaging_date.value();
        const auto             year_string  = date_string.substr(0, 2);
        const auto             month_string = date_string.substr(2, 2);
        const auto             day_string   = date_string.substr(4, 2);

        const auto year          = atoi(std::string(year_string).c_str());
        const auto adjusted_year = 2000 + year;
        const auto month         = atoi(std::string(month_string).c_str());
        const auto day           = atoi(std::string(day_string).c_str());
        return QDate(adjusted_year, month, day);
    }

    bool SystemInfoModel::IsPowerDpmWritable() const
    {
        return info_.os.config.power_dpm_writable;
    }

    QVariant SystemInfoModel::data(const QModelIndex& index, const int role) const
    {
        if (!index.isValid() || role != Qt::DisplayRole)
        {
            return {};
        }

        const auto item = static_cast<SystemInfoItem*>(index.internalPointer());
        if (index.column() == 0)
        {
            return item->GetKey();
        }

        return item->GetValue();
    }

    QVariant SystemInfoModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
    {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        {
            switch (section)
            {
            case kSystemInfoItemColumnKey:
                return {"Key"};
            case kSystemInfoItemColumnValue:
                return {"Value"};
            default:
                return {};
            }
        }

        return {};
    }

    QModelIndex SystemInfoModel::index(const int row, const int column, const QModelIndex& parent) const
    {
        const SystemInfoItem* parent_item;

        if (!parent.isValid())
        {
            parent_item = root_.get();
        }
        else
        {
            parent_item = static_cast<SystemInfoItem*>(parent.internalPointer());
        }

        if (const SystemInfoItem* child = parent_item->GetChild(row); child != nullptr)
        {
            return createIndex(row, column, child);
        }

        return {};
    }

    QModelIndex SystemInfoModel::parent(const QModelIndex& index) const
    {
        if (!index.isValid())
        {
            return {};
        }

        const SystemInfoItem* child  = static_cast<SystemInfoItem*>(index.internalPointer());
        const SystemInfoItem* parent = child->GetParent();

        if (parent == root_.get())
        {
            return {};
        }

        return createIndex(parent->GetRow(), 0, parent);
    }

    int SystemInfoModel::rowCount(const QModelIndex& parent) const
    {
        const SystemInfoItem* parent_item;
        if (!parent.isValid())
        {
            parent_item = root_.get();
        }
        else
        {
            parent_item = static_cast<SystemInfoItem*>(parent.internalPointer());
        }

        if (parent_item != nullptr)
        {
            return parent_item->GetChildCount();
        }

        return 0;
    }

    int SystemInfoModel::columnCount(const QModelIndex& parent) const
    {
        Q_UNUSED(parent)

        return kSystemInfoItemColumnCount;
    }

    bool SystemInfoModel::ExportInfo(const QString& output_path) const
    {
        if (json_data_.isEmpty())
        {
            RDP_LOG_WARN("Could not export out system info because the system info json was blank.");
            return false;
        }

        const QDir    directory(output_path);
        const QString file_path = directory.absoluteFilePath(kJsonExportFilename);

        QFile file(file_path);
        if (!file.open(QIODevice::ReadWrite))
        {
            RDP_LOG_ERROR("Could not export out system info because the file could not be opened for read write.");
            return false;
        }

        QTextStream stream(&file);
        stream << json_document_.toJson(QJsonDocument::Indented);
        file.close();

        return true;
    }

    void SystemInfoModel::FillBugReport(BugReport& report) const
    {
        if (!has_info_)
        {
            report.was_connected = false;
            return;
        }

        const auto& system_info                 = info_;
        report.driver_packaging_version         = system_info.driver.packaging_version.c_str();
        report.operating_system_name            = system_info.os.name.c_str();
        report.operating_system_description     = system_info.os.desc.c_str();
        report.gpu_open_interface_major_version = system_info.devdriver.major_version;

        for (const auto& gpu_info : system_info.gpus)
        {
            const auto id_info = gpu_info.asic.id_info;
            report.gpus.emplace_back(BugReportGpu{gpu_info.name.c_str(), id_info.device, id_info.revision, id_info.family, id_info.gfx_engine});
        }

        report.was_connected = true;
    }

    void SystemInfoModel::NetBusDisconnect()
    {
        beginResetModel();
        root_.reset(nullptr);
        json_data_ = "";
        info_      = {};
        has_info_  = false;
        endResetModel();

        emit SystemInfoChanged(json_data_);
        emit IsDataValidChanged(false);
    }

    static QString ToCamelCase(const QString& text)
    {
        QStringList result;
        for (auto parts = text.split(' '); auto& part : parts)
        {
            result << part.at(0).toUpper() + part.mid(1);
        }
        return result.join(" ");
    }

    QFormLayout* SystemInfoModel::GetFormLayout() const
    {
        // Build a form layout for each section of the system info
        const auto form_layout = new QFormLayout;

        constexpr int spacing = 15;

        // Operating system
        QLabel* os_layout_header = BuildLabel("<b>Host System</b>");
        form_layout->addRow(os_layout_header, new QLabel);
        form_layout->addRow(BuildLabel("OS Name:"), BuildLabel(GetOsName()));
        form_layout->addRow(BuildLabel("OS Description:"), BuildLabel(GetOsDescription()));
        form_layout->addRow(BuildLabel("Hostname:"), BuildLabel(QString::fromStdString(info_.os.hostname)));
        if (!info_.os.memory.type.empty())
        {
            form_layout->addRow(BuildLabel("Physical memory type:"), BuildLabel(info_.os.memory.type.c_str()));
        }
        form_layout->addRow(BuildLabel("Physical memory size:"), BuildLabel(Formatting::FormatBytesPow2(info_.os.memory.physical, 1)));
        form_layout->addRow(BuildLabel("Swap memory size:"), BuildLabel(Formatting::FormatBytesPow2(info_.os.memory.swap, 1)));
        form_layout->addItem(new QSpacerItem(0, 10));

        // Drivers
        const auto& drivers_to_display = info_.drivers.empty() ? std::vector<system_info_utils::DriverInfo>{info_.driver} : info_.drivers;
        for (size_t i = 0; i < drivers_to_display.size(); i++)
        {
            const auto& driver_info = drivers_to_display[i];

            const QString header_text          = drivers_to_display.size() > 1 ? QString("<b>Driver %1</b>").arg(i) : QString("<b>Driver</b>");
            QLabel*       driver_layout_header = BuildLabel(header_text);
            form_layout->addRow(driver_layout_header, new QLabel);
            form_layout->addRow(BuildLabel("Name:"), BuildLabel(QString::fromStdString(driver_info.name)));
            form_layout->addRow(BuildLabel("Description:"), BuildLabel(QString::fromStdString(driver_info.description)));
            if (!driver_info.packaging_version.empty())
            {
                form_layout->addRow(BuildLabel("Packaging version:"), BuildLabel(QString::fromStdString(driver_info.packaging_version)));
            }
            if (driver_info.packaging_date && !driver_info.packaging_date->empty())
            {
                const std::string_view date_string  = driver_info.packaging_date.value();
                const auto             year_string  = date_string.substr(0, 2);
                const auto             month_string = date_string.substr(2, 2);
                const auto             day_string   = date_string.substr(4, 2);
                const auto             year         = 2000 + atoi(std::string(year_string).c_str());
                const auto             month        = atoi(std::string(month_string).c_str());
                const auto             day          = atoi(std::string(day_string).c_str());
                const QDate            date(year, month, day);
                form_layout->addRow(BuildLabel("Packaging date:"), BuildLabel(date.toString()));
            }
            if (!driver_info.software_version.empty())
            {
                form_layout->addRow(BuildLabel("Software version:"), BuildLabel(QString::fromStdString(driver_info.software_version)));
            }

            form_layout->addItem(new QSpacerItem(0, spacing));
        }

        // CPUs
        for (size_t i = 0; i < info_.cpus.size(); i++)
        {
            const auto& cpu = info_.cpus[i];

            QLabel* cpu_layout_header = BuildLabel(QString("<b>CPU %1</b>").arg(i));
            form_layout->addRow(cpu_layout_header, new QLabel);
            form_layout->addRow(BuildLabel("Name:"), BuildLabel(QString::fromStdString(cpu.name)));
            form_layout->addRow(BuildLabel("Architecture:"), BuildLabel(QString::fromStdString(cpu.architecture)));
            form_layout->addRow(BuildLabel("Vendor ID:"), BuildLabel(QString::fromStdString(cpu.vendor_id)));
            if (!cpu.cpu_id.empty())
            {
                form_layout->addRow(BuildLabel("CPU ID:"), BuildLabel(QString::fromStdString(cpu.cpu_id)));
            }
            if (!cpu.device_id.empty())
            {
                form_layout->addRow(BuildLabel("Device ID:"), BuildLabel(QString::fromStdString(cpu.device_id)));
            }
            form_layout->addRow(BuildLabel("Physical core count:"), BuildLabel(QString("%1").arg(cpu.num_physical_cores)));
            form_layout->addRow(BuildLabel("Logical core count:"), BuildLabel(QString("%1").arg(cpu.num_logical_cores)));
            form_layout->addRow(BuildLabel("Speed:"), BuildLabel(Formatting::MHzToGHz(cpu.max_clock_speed)));
            if (!cpu.virtualization.empty())
            {
                form_layout->addRow(BuildLabel("Virtualization:"), BuildLabel(QString::fromStdString(cpu.virtualization)));
            }

            form_layout->addItem(new QSpacerItem(0, spacing));
        }

        // GPUs
        for (size_t i = 0; i < info_.gpus.size(); i++)
        {
            constexpr int inner_spacing                                     = 3;
            const auto& [name, pci, asic, memory, big_sw, gpu_driver_index] = info_.gpus[i];

            QLabel* gpu_layout_header = BuildLabel(QString("<b>GPU %1</b>").arg(i));
            form_layout->addRow(gpu_layout_header, new QLabel);
            form_layout->addRow(BuildLabel("Name:"), BuildLabel(QString::fromStdString(name)));

            if (gpu_driver_index.has_value())
            {
                if (gpu_driver_index.value() < info_.drivers.size())
                {
                    const auto& gpu_driver = info_.drivers[gpu_driver_index.value()];
                    if (!gpu_driver.packaging_version.empty())
                    {
                        form_layout->addRow(BuildLabel("Driver version:"), BuildLabel(QString::fromStdString(gpu_driver.packaging_version)));
                    }
                }
            }
            else if (!info_.driver.packaging_version.empty())
            {
                form_layout->addRow(BuildLabel("Driver version:"), BuildLabel(QString::fromStdString(info_.driver.packaging_version)));
            }
            form_layout->addRow(BuildLabel("Shader engine clock frequency (min):"), BuildLabel(Formatting::FormatHertz(asic.engine_clock_hz.min)));
            form_layout->addRow(BuildLabel("Shader engine clock frequency (max):"), BuildLabel(Formatting::HzToMHz(asic.engine_clock_hz.max)));
            form_layout->addRow(BuildLabel("Timestamp frequency:"), BuildLabel(Formatting::FormatHertz(asic.gpu_counter_freq)));
            form_layout->addRow(BuildLabel("Family:"), BuildLabel(FormatHex(asic.id_info.family)));
            form_layout->addRow(BuildLabel("Device ID:"), BuildLabel(FormatHex(asic.id_info.device)));
            form_layout->addRow(BuildLabel("Revision:"), BuildLabel(FormatHex(asic.id_info.revision)));
            form_layout->addRow(BuildLabel("eRev:"), BuildLabel(FormatHex(asic.id_info.e_rev)));

            form_layout->addItem(new QSpacerItem(0, inner_spacing));

            // Memory
            QLabel* memory_layout_header = BuildLabel("<b>Memory</b>");
            form_layout->addRow(memory_layout_header, new QLabel);
            if (!memory.type.empty())
            {
                form_layout->addRow(BuildLabel("Type:"), BuildLabel(QString::fromStdString(memory.type).toUpper()));
            }
            form_layout->addRow(BuildLabel("Bandwidth:"), BuildLabel(Formatting::FormatBandwidth(memory.bandwidth, 0)));
            form_layout->addRow(BuildLabel("Bus bit width:"), BuildLabel(QString("%1").arg(memory.bus_bit_width)));
            form_layout->addRow(BuildLabel("Clock frequency (min):"), BuildLabel(Formatting::FormatHertz(memory.mem_clock_hz.min)));
            form_layout->addRow(BuildLabel("Clock frequency (max):"), BuildLabel(Formatting::HzToMHz(memory.mem_clock_hz.max)));
            form_layout->addRow(BuildLabel("Operations per clock:"), BuildLabel(QString("%1").arg(memory.mem_ops_per_clock)));

            for (const auto& heap : memory.heaps)
            {
                const auto heap_type = ToCamelCase(QString::fromStdString(heap.heap_type));
                form_layout->addRow(BuildLabel(heap_type + " heap size:"), BuildLabel(Formatting::FormatBytesPow2(heap.size)));
            }

            form_layout->addItem(new QSpacerItem(0, inner_spacing));

            // PCI
            QLabel* pci_layout_header = BuildLabel("<b>PCI</b>");
            form_layout->addRow(pci_layout_header, new QLabel);
            form_layout->addRow(BuildLabel("Bus:"), BuildLabel(QString("%1").arg(pci.bus)));
            form_layout->addRow(BuildLabel("Device:"), BuildLabel(QString("%1").arg(pci.device)));
            form_layout->addRow(BuildLabel("Function:"), BuildLabel(QString("%1").arg(pci.function)));

            form_layout->addItem(new QSpacerItem(0, inner_spacing));

            PciLocation pci_location   = {};
            pci_location.bits.bus      = pci.bus;
            pci_location.bits.device   = pci.device;
            pci_location.bits.function = pci.function;

            form_layout->addRow(BuildLabel("PCI/GPU ID:"), BuildLabel(QString("%1").arg(pci_location.u32All)));

            form_layout->addItem(new QSpacerItem(0, inner_spacing));

            form_layout->addRow(BuildLabel("Settings File PCI Name:"), BuildLabel(QString::fromStdString(PciLocationToString(&pci_location))));
            form_layout->addItem(new QSpacerItem(0, inner_spacing));

            if (!IsLinux())
            {
                QLabel* bigsw_layout_header = BuildLabel("<b>Big SW</b>");
                form_layout->addRow(bigsw_layout_header, new QLabel);
                form_layout->addRow(BuildLabel("Major:"), BuildLabel(QString("%1").arg(big_sw.major)));
                form_layout->addRow(BuildLabel("Minor:"), BuildLabel(QString("%1").arg(big_sw.minor)));
                form_layout->addRow(BuildLabel("Misc:"), BuildLabel(QString("%1").arg(big_sw.misc)));

                form_layout->addItem(new QSpacerItem(0, spacing));
            }
        }

        return form_layout;
    }

    QString SystemInfoModel::FormatHex(const uint32_t value)
    {
        return QString::number(value, 16).toUpper();
    }

}  // namespace rdp
