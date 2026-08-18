// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation of a general purpose commandline parser.

#include "command_line_parser.h"

#ifndef WIN32
#include <strings.h>
#include <climits>
#endif

CommandLineParser::CommandLineParser(int argc, char** argv)
    : help_option_found_(false)
{
    // Save the arguments on the commandline.  Skip the first item (the executable name).
    argc--;
    int index = 1;
    while (argc != 0)
    {
        command_line_arguments_.emplace_back(argv[index]);
        index++;
        argc--;
    }
}

bool CommandLineParser::IsHelpRequested() const
{
    return help_option_found_;
}

void CommandLineParser::SetHelpOption(const char* name, const char* description)
{
    if (name != nullptr)
    {
        help_option_name_ = name;
    }

    if (description != nullptr)
    {
        help_option_description_ = description;
    }
}

void CommandLineParser::AddParameter(CommandLineParameter* parameter)
{
    defined_parameters_.emplace_back(parameter);
}

bool CommandLineParser::ProcessSingleArgument(std::vector<std::string>::iterator& iter, std::string& error)
{
    if (*iter == help_option_name_)
    {
        help_option_found_ = true;
        return true;
    }

    CommandLineParameter* parameter = MatchParameter(*iter);
    if (parameter == nullptr)
    {
        error += "Invalid parameter - '" + *iter + "'.\n";
        return false;
    }

    std::string       argument;
    const std::string parameter_name = *iter;

    if (!parameter->IsFlag())
    {
        ++iter;
        if (iter == command_line_arguments_.end())
        {
            error += "Missing value for parameter '" + parameter_name + "'.\n";
            return false;
        }
        argument = *iter;
    }

    if (parameter->IsParsed())
    {
        error += "Parameter '" + parameter_name + "' listed more than once.\n";
        return false;
    }

    const bool parse_parameter_result = parameter->Parse(argument);
    if (!parse_parameter_result)
    {
        error.append("Invalid value '");
        error.append(argument);
        error.append("' for parameter '");
        error.append(parameter_name);
        error.append("'.\n");
    }
    return parse_parameter_result;
}

bool CommandLineParser::Parse()
{
    error_string_.clear();
    bool parse_successful = true;

    for (auto cmd_args_iterator = command_line_arguments_.begin(); cmd_args_iterator != command_line_arguments_.end(); ++cmd_args_iterator)
    {
        if (std::string arg_error; !ProcessSingleArgument(cmd_args_iterator, arg_error))
        {
            error_string_ += arg_error;
            parse_successful = false;
            if (cmd_args_iterator == command_line_arguments_.end())
            {
                break;
            }
        }
    }

    if (parse_successful)
    {
        // Make sure all required parameters are present.
        for (const auto& defined_parameter : defined_parameters_)
        {
            if (defined_parameter->IsRequired() && !defined_parameter->IsParsed())
            {
                error_string_ += "A required parameter, '" + defined_parameter->GetName() + "', is missing.\n";
                parse_successful = false;
                break;
            }
        }
    }

    return parse_successful;
}

CommandLineParameter* CommandLineParser::MatchParameter(const std::string& parameter_name) const
{
    CommandLineParameter* parameter = nullptr;
    for (auto* defined_parameter : defined_parameters_)
    {
#ifdef WIN32
        if (_strnicmp(defined_parameter->GetName().c_str(), parameter_name.c_str(), _MAX_PATH) == 0)
#else
        if (strncasecmp(defined_parameter->GetName().c_str(), parameter_name.c_str(), PATH_MAX) == 0)
#endif
        {
            parameter = defined_parameter;
            break;
        }
    }
    return parameter;
}

const std::string& CommandLineParser::ErrorString() const
{
    return error_string_;
}

const std::string& CommandLineParser::HelpString() const
{
    static std::string help_string;
    const std::string  indent_string("          ");
    help_string = "Options:\n";

    for (const auto& defined_parameter : defined_parameters_)
    {
        if (!defined_parameter->GetDescription().empty())
        {
            help_string += indent_string + defined_parameter->GetDescription() + "\n";
        }
    }
    help_string += indent_string + help_option_description_ + "\n";
    return help_string;
}
