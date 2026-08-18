// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Command line parameter class definition

#ifndef RDP_SOURCE_SERVICE_CLI_COMMAND_LINE_PARAMETER_H_
#define RDP_SOURCE_SERVICE_CLI_COMMAND_LINE_PARAMETER_H_

#include <string>

class CommandLineParameter
{
public:
    enum class Type
    {
        kValue,
        kFlag
    };

    /// @brief Constructor
    /// @param [in] name The name of the parameter, including any prefix characters (e.g. "--" or "/").
    /// @param [in] description Help information for the parameter.
    /// @param [in] required True indicates that the parameter must be specified on the command line
    /// otherwise an error is reported.  False indicates that the parameter is optional.  The default
    /// is false.
    /// @param [in] type Parameter type, either value type or flag type
    /// @param [in] default_value A string value that specifies the default value for optional parameters
    /// that require an argument.  The default is a null string.
    CommandLineParameter(const char* name, const char* description, bool required = false, Type type = Type::kValue, const char* default_value = "");

    /// @brief Destructor.
    virtual ~CommandLineParameter() = default;

    /// @brief Parses a command line parameter and string value.
    /// @param [in] value The argument following the parameter on the commandline.
    /// @return True if the value string is not blank or if the parameter is a flag.
    /// Otherwise returns false. Override this method for parsing specific argument types.
    virtual bool Parse(const std::string& value);

    /// @brief Returns the value associated with the parameter (either the default value or
    /// the parsed value).
    /// @return The value string for the parameter.
    [[nodiscard]] const std::string& Value() const;

    /// @brief Indicates that the parameter value is valid.  The Parse() method must be
    /// called before retrieving this state.
    /// @return The value of the valid_ member variable.
    [[nodiscard]] bool IsValid() const;

    /// @brief Indicates that the parameter and value have been parsed.
    /// @return The value of the parsed_ member variable.
    [[nodiscard]] bool IsParsed() const;

    /// @brief Indicates that the parameter doesn't have an argument associated with it.
    /// @return The value of the flag_ member variable.
    [[nodiscard]] bool IsFlag() const;

    /// @brief Indicates that the parameter is required to be specified on the commandline.
    /// @return The value of the required_ member variable.
    [[nodiscard]] bool IsRequired() const;

    /// @brief Gets the description of the parameter for display in a help message.
    /// @return The value of the description_ member variable.
    [[nodiscard]] const std::string& GetDescription() const;

    /// @brief Gets the name of the commandline parameter.
    /// @return The value of the name_ member variable.
    [[nodiscard]] const std::string& GetName() const;

protected:
    std::string name_;         ///< The name of the parameter.
    std::string description_;  ///< The description of the parameter.
    std::string value_;        ///< The parsed value of the parameter (or default value).
    Type        type_;         ///< parameter type
    bool        parsed_;       ///< Indicates that the parameter has been parsed.
    bool        valid_;        ///< Indicates that the parameter's parsed value is valid.
    bool        required_;     ///< Indicates that the parameter is required to be on the commandline.
};

/// @brief Defines a 16 bit integer parameter type
class Int16CommandLineParameter : public CommandLineParameter
{
public:
    /// Constructor
    /// @param [in] name The name of the parameter, including any prefix characters (e.g. "--" or "/").
    /// @param [in] description Help information for the parameter.
    /// @param [in] required True indicates that the parameter must be specified on the command line
    /// otherwise an error is reported.  False indicates that the parameter is optional.  The default
    /// is false.
    /// @param [in] default_value A 16 bit integer value that specifies the default value for optional parameters
    /// The default value is 0.
    Int16CommandLineParameter(const char* name, const char* description, bool required = false, int default_value = 0);

    /// @brief Destructor.
    ~Int16CommandLineParameter() override = default;

    /// @brief Parses a command line parameter into 16-bit integer value
    /// @param [in] value The 16 bit integer argument following the parameter on the commandline.
    /// @return True if the value string is a valid 16 bit integer. Otherwise returns false.
    bool Parse(const std::string& value) override;

    /// @brief Gets the parameter value as 16-bit integer
    /// @return integer value
    [[nodiscard]] int GetIntValue() const;

private:
    int int_value_;  ///< The 16 bit value parsed on the commandline associated with the parameter.
};
#endif
