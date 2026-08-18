// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for a generic utility view model.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_MODEL_UTILITY_USERDATA_VIEW_MODEL_H_
#define RDP_SOURCE_MODULES_COMMON_INC_MODEL_UTILITY_USERDATA_VIEW_MODEL_H_

#include <QObject>
#include <QString>

/// @brief Slots and signals definition for the base utility view model.
///
/// The BaseUtilityViewModel is generic to wrap over the different kinds of userdata structs.
/// Qt doesn't support the Q_OBJECT macro on classes with templates. This class contains
/// all the signals and slots used by BaseUtilityViewModel as a workaround to the Qt limitation.
///
/// Note: The prelaunch settings editable state is now determined by the view layer by checking
/// if a process is connected (status.pid == 0) and communicated via Qt signals/slots.
class UserdataViewModel : public QObject
{
    Q_OBJECT

public:
    /// @brief Destructor.
    ~UserdataViewModel() override = default;

    /// @brief Should be called when a view binds to the model.
    virtual void OnBind() = 0;

    /// @brief Initializes the userdata to default, applies it, and calls OnUserdataChanged().
    virtual void InitializeDefaultsAndApply() = 0;

    /// @brief Called when the userdata is received from RDP.
    /// @param [in] data The raw bytes of the data.
    virtual bool ReceiveUserData(const std::string& data) = 0;

    /// @brief Loads serialized data.
    /// @param [in] serialized_data The serialized data.
    /// @return true if loading the data was successful.
    virtual bool LoadSerializedData(const std::string& serialized_data) = 0;

signals:

    /// @brief Emitted when the output path for traces changes.
    /// @param [in] output_path The new output path.
    void OnOutputPathChanged(const QString& output_path);

    /// @brief Emitted when the output path is validated.
    /// @param [in] show_warning_text true if warning text saying that the path is invalid should be displayed.
    void OnOutputPathValidated(bool show_warning_text);

public slots:

    /// @brief Called when the output path changes.
    /// @param [in] output_path The new output path.
    void OutputPathChanged(const QString& output_path);

protected:
    /// @brief Called when the output path changes.
    ///
    /// Any logic to update that path should be performed here.
    /// @param [in] output_path The new output path.
    virtual void HandleOutputPathChanged(const QString& output_path) = 0;
};

#endif
