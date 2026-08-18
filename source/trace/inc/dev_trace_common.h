// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Global declares for LibDevTrace.

#ifndef RDP_SOURCE_TRACE_INC_DEV_TRACE_COMMON_H_
#define RDP_SOURCE_TRACE_INC_DEV_TRACE_COMMON_H_

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>

#include <dd_common_api.h>
#include <system_info_reader.h>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#else
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define DEV_TRACE_ASSERT(x) assert((x));

#define DEV_TRACE_ASSERT_MSG(x, msg)   \
    if (!(x))                          \
    {                                  \
        std::cerr << msg << std::endl; \
        assert((x));                   \
    }

namespace dipper
{
    struct Container;
}

namespace devtrace
{

    /// @brief Result of trace operations.
    enum class Result : uint8_t
    {
        kSuccess,             ///< The operation was successful.
        kFailure,             ///< The operation was not successful.
        kUnsupported,         ///< The operation was unsupported.
        kNotFound,            ///< Something required by the operation could not be found.
        kExecutableNotFound,  ///< The executable required to perform the operation was not found.
        kNotReady             ///< The operation was unavailable due to ready state.
        // Other things we need
        // ...
    };

    /// @brief The various reasons something was disabled.
    enum class DisabledReason : uint8_t
    {
        kEnabled = 0,          ///< The thing is enabled.
        kNoReason,             ///< The thing is disabled, but there is no reason.
        kEncounteredError,     ///< An unexpected error was encountered, so the thing was disabled.
        kHardwareUnsupported,  ///< The thing required hardware support that was not available.
        kOsUnsupported,        ///< The thing required operating system support that was not available.
        kDriverUnsupported,    ///< The thing required driver support that was not available.
        kApiUnsupported        ///< The thing required API support that was not available.
    };

    /// @brief Provides the user-readable string describing the disabled reason.
    /// @param [in] reason The reason to describe.
    /// @return A string describing the reason.
    inline std::string DisabledReasonToString(const DisabledReason reason)
    {
        switch (reason)
        {
        case DisabledReason::kEnabled:
            return "";
        case DisabledReason::kApiUnsupported:
            return "The current API(s) are not supported.";
        case DisabledReason::kEncounteredError:
            return "An unexpected error was encountered.";
        case DisabledReason::kHardwareUnsupported:
            return "The current hardware is not supported.";
        case DisabledReason::kDriverUnsupported:
            return "The current driver is not supported.";
        case DisabledReason::kOsUnsupported:
            return "The current operating system is not supported.";
        case DisabledReason::kNoReason:
            return "This could be a result of incompatible hardware, operating system, API or driver.";
        default:
            return "Unknown Reason";
        }
    }

    /// @brief Current stage of a trace.
    enum class TraceSourceStage : uint8_t
    {
        kDisconnected = 0,       ///< Not currently connected to an application.
        kIdle,                   ///< Tracing is allowed, but is not in progress.
        kWaitingToBeginCapture,  ///< The trace source is ready to capture, but is waiting for an external request to proceed (delayed capture).
        kCapturing,              ///< The request to trace has been sent and the trace is being taken.
        kDumping,                ///< The trace is completed and is streaming from the GPU down to the CPU and being written to disk.
        kProcessing,             ///< The file has been dumped, but some post-processing is being performed.
        kDone,                   ///< The trace completed successfully, but no more traces can be taken.
        kDisabled,               ///< Tracing is disabled for some reason, likely an incompatibility with the driver or the app.
        kError,                  ///< The trace errored, no further traces can be taken.
        kBusy                    ///< The trace source is busy with some operation
    };

    /// @brief The different APIs that traces can be taken from.
    enum class Api : uint8_t
    {
        kUnknown,  ///< Unknown API. If the client has connected, something has likely gone wrong.

        kDirectX12,  ///< DirectX12.
        kDirectX11,  ///< DirectX11.
        // DirectX 10 and 11 use the same driver, so both will be reported as DX11
        kDirectX9,  ///< DirectX9.

        kVulkan,  ///< Any version of Vulkan.
        kOpenCl,  ///< Any version of OpenCL.
        kHip,     ///< Any version of HIP.
        kOpenGl,  ///< Any version of OpenGL.
        kCount    ///< Number of total APIs.
    };

    /// @brief Determines whether a particular API is a compute API or not.
    /// @param [in] api The API to decide whether it is compute.
    /// @return true if the API is a compute API, false otherwise.
    bool IsComputeApi(Api api);

    /// @brief Gets the human-readable name for the API.
    /// @param [in] api The API to get the human-readable name for.
    /// @return The human-readable name for the API (UTF-8).
    [[nodiscard]] std::string GetHumanReadableName(Api api);

    /// @brief The status of a completed trace.
    enum class TraceCompletionStatus : uint8_t
    {
        kUnknown,         ///< The completion status is unknown.
        kCompleted,       ///< The trace completed successfully.
        kNeedProcessing,  ///< The trace needs post-processing.
        kError,           ///< There was an error taking the trace.
        kAborted          ///< The trace was aborted.
    };

    /// @brief The completion result of a trace.
    struct TraceCompletionResult
    {
        TraceCompletionStatus status;             ///< The status of the trace.
        std::string           path;               ///< The path of the file written.
        DDConnectionId        umd_connection_id;  ///< The UMD connection that created this trace.
    };

    /// @brief Gets the API from the driver description.
    /// @param [in] client_driver_description The client driver description.
    /// @return The API
    inline Api GetApiFromDriverDescription(const char* client_driver_description)
    {
        if (client_driver_description != nullptr)
        {
            const std::string_view client_driver_description_str(client_driver_description);
            if (client_driver_description_str.find("DirectX12") != std::string::npos)
            {
                return Api::kDirectX12;
            }

            if (client_driver_description_str.find("DirectX10/11") != std::string::npos)
            {
                // DX11 and DX10 use the same driver
                return Api::kDirectX11;
            }

            if (client_driver_description_str.find("DirectX9") != std::string::npos)
            {
                return Api::kDirectX9;
            }

            if (client_driver_description_str.find("Vulkan") != std::string::npos)
            {
                return Api::kVulkan;
            }

            if (client_driver_description_str.find("OpenCL") != std::string::npos)
            {
                return Api::kOpenCl;
            }

            if (client_driver_description_str.find("HIP") != std::string::npos)
            {
                return Api::kHip;
            }

            if (client_driver_description_str.find("OpenGL") != std::string::npos)
            {
                return Api::kOpenGl;
            }
        }

        return Api::kUnknown;
    }

    inline bool IsDriverTooOld(const system_info_utils::DriverInfo& driver, const uint32_t desired_major_version, const uint32_t desired_minor_version)
    {
        return driver.packaging_version_major < desired_major_version ||
               (driver.packaging_version_major == desired_major_version && driver.packaging_version_minor < desired_minor_version);
    }

#ifdef _WIN32
    inline void SetCurrentThreadName(LPCWCHAR name)
#else
    inline void SetCurrentThreadName(const char* name)
#endif
    {
#ifdef _WIN32
        std::ignore = SetThreadDescription(GetCurrentThread(), name);
#else
        pthread_t self = pthread_self();
        pthread_setname_np(self, name);
#endif
    }

    /// @brief Gets the Dipper container with concrete implementations registered.
    /// @return The default dipper container.
    dipper::Container GetDefaultDipperContainer();

}  // namespace devtrace

#endif
