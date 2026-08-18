// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for the model that manages all of the presets.

#ifndef RDP_SOURCE_FRONTEND_MODELS_PRESET_MODEL_H_
#define RDP_SOURCE_FRONTEND_MODELS_PRESET_MODEL_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <QAbstractItemModel>
#include <QDateTime>
#include <QStringList>

class QFile;
struct DevToolsModule;

namespace rdp
{
    /// @brief The state of an individual module.
    struct ModuleState
    {
        bool        enabled = false;  ///< true if the module is enabled, false otherwise.
        std::string serialized_data;  ///< The module's serialized data.
    };

    /// @brief A preset that contains which modules are enabled and their data.
    struct ModulePreset
    {
        QString                                      name;                 ///< The name of the preset.
        std::unordered_map<std::string, ModuleState> modules;              ///< The data for the modules.
        bool                                         is_built_in = false;  ///< true if the preset is one that comes with RDP.

        QDateTime last_loaded;  ///< The time the preset was last loaded.
    };

    /// @brief Model that manages presets.
    class PresetModel : public QObject
    {
        Q_OBJECT
    public:
        /// @brief Constructor
        /// @param [in] module_model The model that manages modules.
        explicit PresetModel(const std::shared_ptr<class ModuleModel>& module_model);

        /// @brief Loads the preset module and also loads the initial preset.
        void Load();

    private:
        /// @brief Loads existing presets from disk.
        void LoadExistingPresets();

        /// @brief Parses the presets file.
        /// @param [in] file The file to parse.
        /// @param [out] out_presets The presets that were loaded from the file.
        /// @param [out] current_modules The current preset.
        /// @param [out] found_current_modules Will be set to true if the current modules were found in the file.
        static void ParsePresetFile(class QFile&               file,
                                    std::vector<ModulePreset>& out_presets,
                                    ModulePreset&              current_modules,
                                    bool*                      found_current_modules = nullptr);

        /// @brief Parses a preset.
        /// @param [in] object The JSON object to parse the preset from.
        /// @param [out] preset The parsed preset.
        static void ParsePreset(const QJsonObject& object, ModulePreset& preset);

        /// @brief Gets the index of the preset with the given name or -1 if it does not exist.
        /// @param [in] preset_name The name of the preset to get.
        /// @return The index of the preset with the given name or -1 if it does not exist.
        int GetPresetIndex(const QString& preset_name);

    public:
        /// @brief Saves the presets to a file.
        void SavePresets();

    private:
        /// @brief Builds a preset from the current modules.
        /// @param [out] out_preset The preset from the current modules.
        void BuildCurrentModulePreset(ModulePreset& out_preset);

        /// @brief Serializes the preset to a JSON object.
        /// @param [in] preset The preset to serialize.
        /// @param [out] object The object to serialize to.

        /// @brief Serializes the preset to JSON.
        /// @param [in] preset The preset to serialize.
        /// @param [out] object The object to serialize to.
        static void SerializePresetToJson(const ModulePreset& preset, class QJsonObject& object);

    public:
        /// @brief Gets the names of all the presets.
        /// @return The names of all the presets.
        [[nodiscard]] QStringList GetPresets() const;

        /// @brief Gets the names of all the built-in presets.
        /// @return The names of all the built-in presets.
        [[nodiscard]] QStringList GetBuiltInPresets() const;

        /// @brief Gets the names of all of the custom presets.
        /// @return The names of all the custom presets.
        [[nodiscard]] QStringList GetCustomPresets() const;

        /// @brief Gets the most recently loaded custom presets.
        /// @return The names of the most recently loaded custom presets.
        [[nodiscard]] QStringList GetRecentCustomPresets() const;

        /// @brief Updates the preset and fills it with the current module data or creates it if it does not exist.
        /// @param [in] preset_name The preset name to update or create.
        /// @return true if the operation was successful, false otherwise.
        bool CreateOrUpdatePreset(const QString& preset_name);

        /// @brief Loads a preset.
        /// @param [in] preset_name The name of the preset to load.
        void LoadPreset(const QString& preset_name);

    private:
        /// @brief Loads a preset.
        /// @param [in] preset The preset to load.
        void LoadPreset(const ModulePreset& preset);

    signals:
        /// @brief Emitted when a preset is loaded.
        void PresetLoaded();

    public:
        /// @brief Deletes the preset with the given name.
        /// @param [in] preset_name The name of the preset to remove.
        void DeletePreset(const QString& preset_name);

    private slots:
        /// @brief Updates the serialized data for the module.
        /// @param [in] module The module to update the data for.
        /// @param [in] serialized_data The serialized data.
        void SerializeModule(const DevToolsModule* module, const std::string& serialized_data);

        /// @brief Called when a module is enabled / disabled.
        void OnModuleStatusChanged();

    private:
        std::shared_ptr<class ModuleModel> module_model_;  ///< The model that manages modules.
        std::vector<ModulePreset>          presets_;       ///< The preset names and their data.

        std::atomic<bool>                            is_loading_preset_;    ///< true if a preset is loading, false otherwise.
        std::unordered_map<std::string, std::string> current_module_data_;  ///< The data for the currently set of modules (not tied to a preset).
    };
}  // namespace rdp

#endif
