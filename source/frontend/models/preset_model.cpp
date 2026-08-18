// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the model that manages all of the presets.

#include "preset_model.h"

#include <algorithm>
#include <ranges>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <common/inc/api/dev_tools_module.h>

#include "module_model.h"
#include "utilities.h"

static constexpr const char* kPresetsFileName         = "presets.json";
static constexpr const char* kCurrentModulesKey       = "current";
static constexpr const char* kPresetsKey              = "presets";
static constexpr const char* kPresetNameKey           = "name";
static constexpr const char* kPresetModulesKey        = "modules";
static constexpr const char* kPresetBuiltinKey        = "builtin";
static constexpr const char* kPresetTimeLastLoadedKey = "last_loaded";
static constexpr const char* kModuleEnabledKey        = "enabled";
static constexpr const char* kModuleDataKey           = "data";

static constexpr const char* kDateTimeFormat = "yyyy-MM-dd hh:mm:ss";

static constexpr uint32_t kMaxRecentPresets = 5;

namespace rdp
{

    PresetModel::PresetModel(const std::shared_ptr<ModuleModel>& module_model)
        : module_model_(module_model)
        , is_loading_preset_(false)
    {
        connect(module_model_.get(), &ModuleModel::SerializeModule, this, &PresetModel::SerializeModule);
        connect(module_model_.get(), &ModuleModel::ModuleStatusChanged, this, &PresetModel::OnModuleStatusChanged);
    }

    void PresetModel::Load()
    {
        LoadExistingPresets();
    }

    void PresetModel::LoadExistingPresets()
    {
        presets_.clear();

        ModulePreset user_current_modules{};
        bool         found_user_current_modules = false;

        const QString file_path = util::GetApplicationDataPath().absoluteFilePath(kPresetsFileName);
        QFile         user_file(file_path);

        ParsePresetFile(user_file, presets_, user_current_modules, &found_user_current_modules);

        std::vector<ModulePreset> default_presets;
        ModulePreset              default_current_modules{};

        QFile default_file(":/default_presets.json");
        ParsePresetFile(default_file, default_presets, default_current_modules);

        // If a new default preset has been added, it needs to be put into the user's preset file
        for (ModulePreset& default_preset : default_presets)
        {
            if (GetPresetIndex(default_preset.name) == -1)
            {
                presets_.push_back(std::move(default_preset));
            }
        }

        Q_ASSERT(!module_model_->AreModulesLocked());
        LoadPreset(found_user_current_modules ? user_current_modules : default_current_modules);
    }

    void PresetModel::ParsePresetFile(QFile& file, std::vector<ModulePreset>& out_presets, ModulePreset& current_modules, bool* found_current_modules)
    {
        if (found_current_modules != nullptr)
        {
            *found_current_modules = false;
        }

        if (!file.open(QIODevice::ReadOnly))
        {
            return;
        }

        const QString file_contents = file.readAll();
        file.close();

        QJsonParseError     parse_error{};
        const QJsonDocument doc = QJsonDocument::fromJson(file_contents.toUtf8(), &parse_error);

        if (parse_error.error != QJsonParseError::NoError)
        {
            return;
        }

        if (!doc.isObject())
        {
            return;
        }

        const QJsonObject root_object  = doc.object();
        const QJsonArray  preset_array = root_object.value(kPresetsKey).toArray();

        for (const QJsonValue& preset_object : preset_array)
        {
            ModulePreset new_preset;
            ParsePreset(preset_object.toObject({}), new_preset);

            out_presets.emplace_back(std::move(new_preset));
        }

        const QJsonObject current_modules_obj = root_object.value(kCurrentModulesKey).toObject({});
        ParsePreset(current_modules_obj, current_modules);

        if (found_current_modules != nullptr)
        {
            *found_current_modules = !current_modules_obj.isEmpty();
        }
    }

    void PresetModel::ParsePreset(const QJsonObject& object, ModulePreset& preset)
    {
        preset.name        = object.value(kPresetNameKey).toString();
        preset.is_built_in = object.value(kPresetBuiltinKey).toBool();

        preset.last_loaded = QDateTime::fromString(object.value(kPresetTimeLastLoadedKey).toString(), kDateTimeFormat);
        if (!preset.last_loaded.isValid())
        {
            preset.last_loaded = QDateTime::currentDateTime();
        }

        const QJsonObject modules = object.value(kPresetModulesKey).toObject({});
        for (const QString& module_name : modules.keys())
        {
            const QJsonObject module_state = modules.value(module_name).toObject({});
            const bool        is_enabled   = module_state.value(kModuleEnabledKey).toBool();
            const std::string data         = module_state.value(kModuleDataKey).toString().toStdString();

            preset.modules.insert({module_name.toStdString(), {is_enabled, data}});
        }
    }

    int PresetModel::GetPresetIndex(const QString& preset_name)
    {
        const auto iterator = std::ranges::find_if(presets_, [&](const ModulePreset& preset) { return preset.name == preset_name; });
        return iterator == presets_.end() ? -1 : static_cast<int>(std::distance(presets_.begin(), iterator));
    }

    void PresetModel::SavePresets()
    {
        // Serialize the current modules
        QJsonObject current_modules{};

        ModulePreset current_preset;
        BuildCurrentModulePreset(current_preset);
        SerializePresetToJson(current_preset, current_modules);

        // Serialize the presets
        QJsonArray presets{};
        for (const ModulePreset& preset : presets_)
        {
            QJsonObject preset_object{};
            SerializePresetToJson(preset, preset_object);

            presets.push_back(preset_object);
        }

        QJsonObject root_object{};
        root_object[kCurrentModulesKey] = current_modules;
        root_object[kPresetsKey]        = presets;

        QJsonDocument doc{};
        doc.setObject(root_object);

        const QString file_path = util::GetApplicationDataPath().absoluteFilePath(kPresetsFileName);
        QFile         file(file_path);

        if (!file.open(QIODevice::WriteOnly))
        {
            return;
        }

        file.write(doc.toJson());
        file.close();
    }

    void PresetModel::BuildCurrentModulePreset(ModulePreset& out_preset)
    {
        out_preset.modules.clear();

        for (const DevToolsModule* module : module_model_->GetModules())
        {
            const std::string& module_name = module->GetModuleName();
            Q_ASSERT(current_module_data_.count(module_name) > 0);

            out_preset.modules.insert({module_name, {module->IsEnabled(), current_module_data_[module_name]}});
        }
    }

    void PresetModel::SerializePresetToJson(const ModulePreset& preset, QJsonObject& object)
    {
        object[kPresetNameKey]           = preset.name;
        object[kPresetBuiltinKey]        = preset.is_built_in;
        object[kPresetTimeLastLoadedKey] = preset.last_loaded.toString(kDateTimeFormat);

        QJsonObject modules_object{};

        for (const auto& pair : preset.modules)
        {
            QJsonObject module_object{};
            module_object[kModuleEnabledKey] = pair.second.enabled;
            module_object[kModuleDataKey]    = pair.second.serialized_data.c_str();

            modules_object[pair.first.c_str()] = module_object;
        }

        object[kPresetModulesKey] = modules_object;
    }

    QStringList PresetModel::GetPresets() const
    {
        QStringList names{};
        for (const ModulePreset& preset : presets_)
        {
            names.push_back(preset.name);
        }

        return names;
    }

    QStringList PresetModel::GetBuiltInPresets() const
    {
        QStringList names{};
        for (const ModulePreset& preset : presets_)
        {
            if (preset.is_built_in)
            {
                names.push_back(preset.name);
            }
        }

        return names;
    }

    QStringList PresetModel::GetCustomPresets() const
    {
        QStringList names{};
        for (const ModulePreset& preset : presets_)
        {
            if (!preset.is_built_in)
            {
                names.push_back(preset.name);
            }
        }

        return names;
    }

    QStringList PresetModel::GetRecentCustomPresets() const
    {
        std::vector<const ModulePreset*> custom_presets;
        for (const ModulePreset& preset : presets_)
        {
            if (!preset.is_built_in)
            {
                custom_presets.push_back(&preset);
            }
        }

        // Sort by most recently loaded since its likely that users will want to keep loading the presets they are using
        std::ranges::sort(custom_presets, [](auto lhs, auto rhs) { return lhs->last_loaded > rhs->last_loaded; });

        if (custom_presets.size() > kMaxRecentPresets)
        {
            custom_presets.erase(custom_presets.begin() + kMaxRecentPresets, custom_presets.end());
        }

        QStringList names{};
        for (auto& preset : custom_presets)
        {
            names.push_back(preset->name);
        }

        return names;
    }

    bool PresetModel::CreateOrUpdatePreset(const QString& preset_name)
    {
        int preset = GetPresetIndex(preset_name);
        if (preset == -1)
        {
            // Create a new preset
            const QString trimmed_name = preset_name.trimmed();
            if (trimmed_name.isEmpty())
            {
                return false;
            }

            presets_.push_back({trimmed_name, {}});
            presets_.back().last_loaded = QDateTime::currentDateTime();

            preset = static_cast<int>(presets_.size()) - 1;
        }

        BuildCurrentModulePreset(presets_[preset]);
        return true;
    }

    void PresetModel::LoadPreset(const QString& preset_name)
    {
        const int preset = GetPresetIndex(preset_name);
        if (preset != -1)
        {
            LoadPreset(presets_[preset]);
            presets_[preset].last_loaded = QDateTime::currentDateTime();

            SavePresets();
        }
    }

    void PresetModel::LoadPreset(const ModulePreset& preset)
    {
        if (module_model_->AreModulesLocked())
        {
            return;
        }

        is_loading_preset_ = true;
        current_module_data_.clear();

        // We start by disabling every module to ensure that if there are any incompatibilities between what is currently enabled
        // and what will be enabled by the preset, it won't stop the preset from being properly loaded.
        for (const DevToolsModule* module : module_model_->GetModules())
        {
            module_model_->SetModuleEnabled(module, false);
        }

        for (const DevToolsModule* module : module_model_->GetModules())
        {
            const std::string module_name = module->GetModuleName();

            // If there is data for the module in the preset, set that data
            if (preset.modules.count(module_name) > 0)
            {
                const ModuleState& module_state = preset.modules.at(module_name);
                current_module_data_.insert({module_name, module_state.serialized_data});

                module_model_->SetModuleEnabled(module, module_state.enabled);

                // Default presets will have no data for the modules, so they should take on the defaults.
                if (module_state.serialized_data.empty())
                {
                    module_model_->ResetModuleToDefault(module);
                }
                else
                {
                    module_model_->SetModuleData(module, module_state.serialized_data);
                }

                continue;
            }

            // The module was missing from the preset, so it is probably new. Keep it disabled and reset it to the default data.
            current_module_data_.insert({module_name, ""});

            module_model_->SetModuleEnabled(module, false);
            module_model_->ResetModuleToDefault(module);
        }

        is_loading_preset_ = false;
        emit PresetLoaded();
    }

    void PresetModel::DeletePreset(const QString& preset_name)
    {
        std::erase_if(presets_, [&](const auto& preset) { return preset.name == preset_name && !preset.is_built_in; });

        SavePresets();
    }

    void PresetModel::SerializeModule(const DevToolsModule* module, const std::string& serialized_data)
    {
        const std::string& module_name = module->GetModuleName();
        Q_ASSERT(current_module_data_.count(module_name) > 0);

        current_module_data_[module_name] = serialized_data;

        if (!is_loading_preset_)
        {
            SavePresets();
        }
    }

    void PresetModel::OnModuleStatusChanged()
    {
        if (!is_loading_preset_)
        {
            SavePresets();
        }
    }

}  // namespace rdp
