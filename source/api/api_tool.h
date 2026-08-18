// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for the class to handle DevDriver tool.

#ifndef RDP_SOURCE_API_CAPTURE_API_TOOL_H_
#define RDP_SOURCE_API_CAPTURE_API_TOOL_H_

#include <memory>
#include <vector>

#include <ddRouter.h>
#include <dd_driver_utils_api.h>
#include <dd_settings_api.h>

#include <dipper.h>

/// @brief Creates and manages a DevDriver tool.
class ApiTool
{
public:
    /// @brief Creates a new tool that connects to the local system synchronously.
    /// @return The new tool if creation was successful, false otherwise.
    static std::unique_ptr<ApiTool> CreateTool();

private:
    /// @brief Constructor.
    ApiTool() = default;

    /// @brief Creates the DDToolApi.
    /// @return true if creation was successful, false otherwise.
    bool CreateToolApi();

    /// @brief Queries the APIs from the API registry.
    /// @return true if getting all the APIs was successful.
    bool QueryApis();

public:
    /// @brief Destructor.
    ~ApiTool();

    /// @brief Destroys the router.
    void Destroy();

    /// @brief Creates a local router.
    /// @return true if creation was successful, false otherwise.
    bool CreateRouter();

    /// @brief Connects to a router.
    /// @param [in] ip The IP to connect to or nullptr to connect to local.
    /// @param [in] port The port to connect to. If connecting to local 0 can be used.
    /// @return true if connection was successful, false otherwise.
    bool ConnectToRouter(const char* ip, uint16_t port);

    /// @brief Disconnects/
    void Disconnect();

    /// @brief Gets the container that has the APIs that were queried from the api registry.
    /// @return The contaienr that contains all of the APIs queried from the registry.
    const dipper::Container& GetApiContainer();

    /// @brief Gets the API registry.
    /// @return The API registry.
    struct DDApiRegistry* GetRegistry();

private:
    struct DDToolApi* tool_api_ = nullptr;                ///< API used to manage DDTool.
    DDRouter          router_   = DD_API_INVALID_HANDLE;  ///< Handle to the router.

    struct DDApiRegistry* api_registry_ = nullptr;  ///< The API registry.
    dipper::Container     container{};              ///< DIP container.
};

#endif
