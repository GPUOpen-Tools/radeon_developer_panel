// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for a base view model for utility views.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_MODEL_UTILITY_BASE_UTILITY_VIEW_MODEL_H_
#define RDP_SOURCE_MODULES_COMMON_INC_MODEL_UTILITY_BASE_UTILITY_VIEW_MODEL_H_

#include <functional>
#include <memory>

#include <QString>

#include <source_userdata.h>

#include <common/inc/model/prelaunch_settings_helper.h>
#include <common/inc/model/utility/userdata_view_model.h>
#include <common/inc/util.h>

/// @brief Base view model implementation for utility views.
/// @tparam UserdataMapper The type of object to use to serialize and deserialize the userdata.
template <typename UserdataMapper>
class BaseUserdataViewModel : public UserdataViewModel
{
    // Q_OBJECT is not used, since it doesn't support templates.
public:
    using UserdataType = UserdataMapper::UserdataType;  ///< The type for the userdata struct.

    /// @brief Constructor.
    /// @param [in] mapper Object used to map userdata to and from JSON.
    /// @param [in] prelaunch_helper Helper for prelaunch settings.
    /// @param [in] output_path_parent_folder The name of the default output path in the user's documents folder.
    /// @param [in] apply_fn The function used to apply changes.
    BaseUserdataViewModel(const std::shared_ptr<UserdataMapper>&          mapper,
                          const std::shared_ptr<PrelaunchSettingsHelper>& prelaunch_helper,
                          const std::string&                              output_path_parent_folder,
                          const std::function<void(const std::string&)>&  apply_fn);

    /// @brief Destructor.
    ~BaseUserdataViewModel() override = default;

    /// @brief Should be called when a view binds to the model.
    ///
    /// If this is overridden, it should still call the superclass version, as this is where
    /// the userdata is loaded initially.
    void OnBind() override;

    /// @brief Initializes the userdata to default, applies it, and calls OnUserdataChanged().
    void InitializeDefaultsAndApply() override;

    /// @brief Loads serialized data.
    /// @param [in] serialized_data The serialized data.
    /// @return true if loading the data was successful.
    bool LoadSerializedData(const std::string& serialized_data) override;

    /// @brief When LoadSerializedData is called, this function provides an opportunity to modify the data if it is invalid.
    ///
    /// This is the place to check if any values are set for internal builds and need to be updated.
    /// @param [in, out] userdata The userdata to modify.
    virtual void ValidateData(UserdataType& userdata);

    /// @brief Performs an update action on the userdata for this model.
    /// @param [in] action The action to perform. The parameters are the current user data (editable) and the cached data.
    void PerformUpdate(const std::function<void(UserdataType&, const UserdataType&)>& action);

protected:
    /// @brief Initialize the default values.
    ///
    /// The base class implementation will handle the trace output path, so the superclass method should be called
    /// if this is overridden.
    /// @param [out] userdata The userdata struct to fill with defaults.
    virtual void InitializeDefaults(UserdataType& userdata);

    /// @brief Called when the entire userdata changes.
    ///
    /// This should emit on updated signals for all the properties on the model.
    /// The base class implementation will handle the trace output path.
    /// @param [in] userdata The new userdata.
    virtual void OnUserdataChanged(const UserdataType& userdata);

    /// @brief Applies the pending changes.
    ///
    /// This will serialize the userdata, pass the data to Dev Driver and then trigger a save.
    /// If this function is overridden, the superclass method should be called AT THE END
    /// of the overridden implementation.
    /// @return true if the pending changes were successfully applied, false otherwise.
    virtual bool Apply();

    /// @brief Called when the output path changes.
    /// @param [in] output_path The new output path.
    void HandleOutputPathChanged(const QString& output_path) final;

    /// @brief Gets the cached userdata.
    /// @return The cached userdata.
    const UserdataMapper::UserdataType& GetCachedUserdata() const;

private:
    /// @brief Verifies the output directory.
    ///
    /// If the output directory is not valid, it will be reset to the cached value.
    /// If the cached value is also invalid, the status label will be signaled to be shown.
    /// @param [in,out] userdata The current userdata. If the output path does not verify, this is modified.
    /// @param [in] cached_userdata The cached userdata. This is the data that was last applied with Dev Driver.
    void ValidateOutputPath(UserdataType& userdata, const UserdataType& cached_userdata);

    std::shared_ptr<UserdataMapper>          mapper_;            ///< Object used to map userdata to and from JSON.
    std::shared_ptr<PrelaunchSettingsHelper> prelaunch_helper_;  ///< Helper for prelaunch settings.

    std::string                             output_path_parent_folder_;  ///< The name of the folder where traces should be put by default.
    std::function<void(const std::string&)> apply_fn_;                   ///< Function used to apply any changes.

    UserdataType userdata_{};         ///< The current userdata.
    UserdataType cached_userdata_{};  ///< The userdata that is currently in the DevDriver store.
};

template <typename UserdataMapper>
bool BaseUserdataViewModel<UserdataMapper>::LoadSerializedData(const std::string& serialized_data)
{
    std::string default_output_path = Util::GetDefaultOutputPath(output_path_parent_folder_.c_str()).toStdString();
    if (mapper_->Parse(serialized_data.c_str(), serialized_data.size() + 1, default_output_path, cached_userdata_).has_value())
    {
        UserdataType unvalidated_userdata = cached_userdata_;
        ValidateData(cached_userdata_);
        userdata_ = cached_userdata_;

        OnUserdataChanged(userdata_);

        const bool is_cached_path_valid = Util::VerifyOutputDirectory(userdata_.output_path.c_str(), false, false);
        emit       OnOutputPathValidated(!is_cached_path_valid);

        if (!(unvalidated_userdata == cached_userdata_))
        {
            Apply();
        }

        return true;
    }

    InitializeDefaultsAndApply();
    return false;
}

template <typename UserdataMapper>
void BaseUserdataViewModel<UserdataMapper>::ValidateData(UserdataType& userdata)
{
    Q_UNUSED(userdata);
}

template <typename UserdataMapper>
BaseUserdataViewModel<UserdataMapper>::BaseUserdataViewModel(const std::shared_ptr<UserdataMapper>&          mapper,
                                                             const std::shared_ptr<PrelaunchSettingsHelper>& prelaunch_helper,
                                                             const std::string&                              output_path_parent_folder,
                                                             const std::function<void(const std::string&)>&  apply_fn)
    : mapper_(mapper)
    , prelaunch_helper_(prelaunch_helper)
    , output_path_parent_folder_(output_path_parent_folder)
    , apply_fn_(apply_fn)
{
}

template <typename UserdataMapper>
void BaseUserdataViewModel<UserdataMapper>::OnBind()
{
    OnUserdataChanged(userdata_);
}

template <typename UserdataMapper>
void BaseUserdataViewModel<UserdataMapper>::InitializeDefaultsAndApply()
{
    PerformUpdate([&](UserdataType& userdata, const UserdataType& cached_userdata) {
        Q_UNUSED(cached_userdata)
        InitializeDefaults(userdata);
    });

    OnUserdataChanged(userdata_);
}

template <typename UserdataMapper>
void BaseUserdataViewModel<UserdataMapper>::InitializeDefaults(UserdataType& userdata)
{
    userdata.output_path = Util::GetDefaultOutputPath(output_path_parent_folder_.c_str()).toStdString();
}

template <typename UserdataMapper>
void BaseUserdataViewModel<UserdataMapper>::OnUserdataChanged(const UserdataType& userdata)
{
    emit OnOutputPathChanged(userdata.output_path.c_str());
}

template <typename UserdataMapper>
void BaseUserdataViewModel<UserdataMapper>::PerformUpdate(const std::function<void(UserdataType&, const UserdataType&)>& action)
{
    action(userdata_, cached_userdata_);
    if (!(userdata_ == cached_userdata_))
    {
        const bool successfully_applied = Apply();
        DEV_TRACE_ASSERT(successfully_applied);
        cached_userdata_ = userdata_;
    }
}

template <typename UserdataMapper>
bool BaseUserdataViewModel<UserdataMapper>::Apply()
{
    auto result = mapper_->Serialize(userdata_);
    if (!result.has_value())
    {
        return false;
    }

    apply_fn_(result.value());

    return true;
}

template <typename UserdataMapper>
void BaseUserdataViewModel<UserdataMapper>::HandleOutputPathChanged(const QString& output_path)
{
    PerformUpdate([&](UserdataType& userdata, const UserdataType& cached_userdata) {
        const std::string new_output_path = output_path.toStdString();
        if (new_output_path == cached_userdata.output_path)
        {
            return;
        }

        userdata.output_path = new_output_path;
        ValidateOutputPath(userdata, cached_userdata);

        // ValidateOutputPath() can mutate userdata.output_path, so we need to use that instead of new_output_path.
        emit OnOutputPathChanged(userdata.output_path.c_str());
    });
}

template <typename UserdataMapper>
void BaseUserdataViewModel<UserdataMapper>::ValidateOutputPath(UserdataType& userdata, const UserdataType& cached_userdata)
{
    const QString new_path    = userdata.output_path.c_str();
    const QString cached_path = cached_userdata.output_path.c_str();

    if (Util::VerifyOutputDirectory(new_path))
    {
        emit OnOutputPathValidated(false);
        return;
    }

    if (new_path != cached_path)
    {
        userdata.output_path = cached_path.toStdString();
    }

    const bool is_cached_path_valid = Util::VerifyOutputDirectory(cached_path, false, false);
    emit       OnOutputPathValidated(!is_cached_path_valid);
}

template <typename UserdataMapper>
const BaseUserdataViewModel<UserdataMapper>::UserdataType& BaseUserdataViewModel<UserdataMapper>::GetCachedUserdata() const
{
    return cached_userdata_;
}

#endif
