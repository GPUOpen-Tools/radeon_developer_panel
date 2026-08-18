// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Definition for view-model binding utility.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_VIEW_MODEL_BINDING_
#define RDP_SOURCE_MODULES_COMMON_INC_VIEW_MODEL_BINDING_

#include <QMetaMethod>
#include <QMetaObject>
#include <QObject>

/// @brief Utility for binding to a model that will disconnect previously connected signals when the model changes.
struct ModelBinder
{
    /// @brief Starts binding to a model.
    ///
    /// All of the connected signals from the previous model will be disconnected.
    void StartBinding();

    /// @brief Connects a signal to a slot as part of the binding to the current model.
    /// @tparam Signal The type of the signal.
    /// @tparam Slot The type of the slot.
    /// @param [in] sender The object that sends the signal.
    /// @param [in] signal The signal to connect to the slot.
    /// @param [in] receiver The object to receive the signal.
    /// @param [in] slot The slot to connect to the signal.
    /// @param [in] type The type of connection to create.
    template <typename Signal, typename Slot>
    void Connect(const QtPrivate::FunctionPointer<Signal>::Object* sender,
                 Signal                                            signal,
                 const QtPrivate::FunctionPointer<Slot>::Object*   receiver,
                 Slot                                              slot,
                 Qt::ConnectionType                                type = Qt::AutoConnection);

private:
    std::vector<QMetaObject::Connection> connections_;  ///< The connections that have been made binding to model_.
};

template <typename Signal, typename Slot>
void ModelBinder::Connect(const typename QtPrivate::FunctionPointer<Signal>::Object* sender,
                          Signal                                                     signal,
                          const typename QtPrivate::FunctionPointer<Slot>::Object*   receiver,
                          Slot                                                       slot,
                          Qt::ConnectionType                                         type)
{
    connections_.push_back(QObject::connect(sender, signal, receiver, slot, type));
}

#endif
