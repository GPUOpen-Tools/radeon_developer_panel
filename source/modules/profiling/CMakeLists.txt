#######################################################################################################################
### Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(DEVTOOLS_MODULE_FOLDER "DevTools Modules/Profiling")

### Module Core #######################################################################################################
add_library(DevToolsProfilingModule STATIC)
devtools_target_options(DevToolsProfilingModule)
target_link_libraries(DevToolsProfilingModule
        PUBLIC
        ddModule
        PRIVATE
        ddCommon
        dd_tools
        system_info
        ddModuleUtil
        MercuryModuleExt
        DevToolsCommonStatic
        QtCustomWidgets
        QtUtils
        Qt::Widgets
        LibDevTrace
        gpa_counters_headers
)
set_target_properties(DevToolsProfilingModule
        PROPERTIES
        FOLDER "DevTools Modules/Gui"
)

target_include_directories(DevToolsProfilingModule
        PRIVATE
        src
        ${EXTERNAL_DIR}
        ${LIBDEVTRACE_DIR}
        PUBLIC
        inc
)

target_sources(DevToolsProfilingModule PRIVATE
        src/gui/profiling_module_definitions.h
        src/gui/profiling_prelaunch_settings_helper.cpp
        src/gui/profiling_prelaunch_settings_helper.h
        src/gui/profiling_view.h
        src/gui/profiling_view_model.h
        src/gui/spm_counter_file_parser.h
        src/gui/profiling_userdata_view_model.h
        src/gui/profiling_userdata_view_model.cpp
        src/gui/profiling_view.cpp
        src/gui/profiling_view_model.cpp
        src/gui/profiling_userdata_view.h
        src/gui/profiling_userdata_view.cpp
        src/gui/rgp_file_validator.h
        src/gui/rgp_file_validator.cpp
        src/Profiling.cpp
        PUBLIC
        inc/Profiling.h
)
