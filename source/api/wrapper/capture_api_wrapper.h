// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  A wrapper class for using the RdpCaptureAPi to setup and capture
///  RGP profiles and RMV traces.

#ifndef RDP_SOURCE_API_EXAMPLES_DEV_DRIVER_WRAPPER_H_
#define RDP_SOURCE_API_EXAMPLES_DEV_DRIVER_WRAPPER_H_

#include <cstdio>

// Forward declaration of implementation class.
class CaptureWrapperImpl;

class CaptureWrapper
{
public:
    /// @brief Get a reference to the instance of the wrapper.
    ///
    /// @return The instance to this wrapper object.
    static CaptureWrapper& Get();

    /// @brief Process the command line arguments of the host application.
    ///
    /// This allows the capture configuration to be changed via the command line.
    ///
    /// @param argc The number of command line arguments.
    /// @param argv The list of command line argument strings.
    void ProcessCommandLine(int argc, char* argv[]);

    /// @brief Initialize the DevDriver depending on the current configuration settings.
    ///
    /// @return true if initialization is sucessful, false otherwise
    bool Init();

    /// @brief Handle anything that needs setting up at device configuration time.
    ///
    /// This is the approximate time the RMV trace starts.
    void DeviceCreated();

    /// @brief Close down the DevDriver gracefully.
    void Close();

    /// @brief Called in the parent graphics application frame update function to update the RMV and RGP capture state.
    ///
    /// @return true if exit app is required, false if not. Allows for apps to self-terminate.
    bool FrameUpdate();

    /// @brief Called in the parent compute application before each dispatch to update the RGP capture state.
    ///
    /// @return true if exit app is required, false if not. Allows for apps to self-terminate.
    bool PreDispatch();

    /// @brief Waits until all traces have finished.
    ///
    /// This will not wait forever, but will return true if everything finished by the timeout.
    /// @return true if capturing was finished, false if this timed out waiting.
    bool WaitUntilFinished();

    /// @brief Display the command line help parameters for RMV/RGP capture to std::cout.
    void DisplayCommandLineHelp();

    /// @brief Logs to both standard out and the logfile if one is open.
    /// @param [in] format The format of the log line message.
    /// @param [in] args The args to replace the format specifiers in the format string.
    template <typename... Args>
    void Log(const char* format, Args... args)
    {
        char log_buffer[2048];
        if constexpr (sizeof...(args) == 0)
        {
            snprintf(log_buffer, sizeof(log_buffer), "%s", format);
        }
        else
        {
            snprintf(log_buffer, sizeof(log_buffer), format, args...);
        }

        LogLine(log_buffer);
    }

private:
    /// @brief Constructor.
    CaptureWrapper();

    /// @brief Destructor.
    ~CaptureWrapper();

    /// @brief Logs an already formatted line.
    void LogLine(const char* log_line);

    CaptureWrapperImpl* impl_;  ///< Pointer to the implementation class.
};

#endif
