// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for trace source provider.

#ifndef SOURCE_API_TRACE_SOURCE_PROVIDER_H_
#define SOURCE_API_TRACE_SOURCE_PROVIDER_H_

#include <functional>
#include <memory>
#include <type_traits>

#include <continuous_trace_source.h>
#include <rgd_trace_source.h>
#include <rgp_trace_source.h>
#include <rmv_trace_source.h>
#include <rra_trace_source.h>
#include <triggerable_trace_source.h>

#include "RdpCaptureApi.h"

/// @brief Allows getting a trace source as a specific type.
/// @tparam T The type to get the trace source as.
template <typename T>
class TraceSourceProvider
{
    /// @brief Creates a getter function for ptr.
    /// @tparam U The concrete type.
    /// @tparam V std::false type if nullptr should be returned.
    template <typename U, typename V>
    struct GetterHelper
    {
        static std::function<std::shared_ptr<T>()> Fn([[maybe_unused]] const std::shared_ptr<U>& ptr)
        {
            return []() { return nullptr; };
        }
    };

    /// @brief Creates a getter function for ptr.
    /// @tparam U The concrete type.
    template <typename U>
    struct GetterHelper<U, std::true_type>
    {
        static std::function<std::shared_ptr<T>()> Fn(const std::shared_ptr<U>& ptr)
        {
            return [=]() { return ptr; };
        }
    };

public:
    /// @brief Constructor.
    /// @tparam U The concrete type of the trace source.
    /// @param [in] ptr The pointer to the concrete trace source.
    template <typename U>
    TraceSourceProvider(const std::shared_ptr<U>& ptr);

    /// @brief Gets the trace source as T or nullptr if it is not convertible.
    /// @return The pointer to the trace source.
    std::shared_ptr<T> Get() const;

private:
    std::function<std::shared_ptr<T>()> fn_;  ///< Getter function for the trace source.
};

template <typename T>
template <typename U>
TraceSourceProvider<T>::TraceSourceProvider(const std::shared_ptr<U>& ptr)
{
    fn_ = GetterHelper<U, typename std::is_convertible<U*, T*>::type>::Fn(ptr);
}

template <typename T>
std::shared_ptr<T> TraceSourceProvider<T>::Get() const
{
    return fn_ ? fn_() : nullptr;
}

/// @brief Implements multiple trace source providers.
/// @tparam Types The types to provide.
template <typename... Types>
class MultiTraceSourceProvider : private TraceSourceProvider<Types>...
{
public:
    /// @brief Constructor.
    /// @tparam U The concrete type of the trace source.
    /// @param [in] ptr The pointer to the concrete trace source.
    template <typename U>
    MultiTraceSourceProvider(const std::shared_ptr<U>& ptr);

    /// @brief Gets the trace source as T or nullptr if it is not convertible.
    /// @tparam T The type to get the trace source as.
    /// @return The pointer to the trace source.
    template <typename T>
    std::shared_ptr<T> GetSource() const;
};

template <typename... Types>
template <typename U>
MultiTraceSourceProvider<Types...>::MultiTraceSourceProvider(const std::shared_ptr<U>& ptr)
    : TraceSourceProvider<Types>(ptr)...
{
}

template <typename... Types>
template <typename T>
std::shared_ptr<T> MultiTraceSourceProvider<Types...>::GetSource() const
{
    static_assert(std::disjunction<std::is_same<T, Types>...>(), "Cannot provide trace sources of the requested type.");
    return TraceSourceProvider<T>::Get();
}

/// @brief Provides a convenient way to get the trace source as any supported type or nullptr if it doesn't convert.
using AllTraceSourceProvider = MultiTraceSourceProvider<devtrace::TriggerableTraceSource,
                                                        devtrace::ContinuousTraceSource,
                                                        devtrace::RgpTraceSource,
                                                        devtrace::RmvTraceSource,
                                                        devtrace::RraTraceSource,
                                                        devtrace::RgdTraceSource>;

#endif
