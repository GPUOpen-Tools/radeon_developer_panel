#######################################################################################################################
### Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(DEVTOOLS_MODULE_FOLDER "DevTools Modules/Crash Analysis")

add_library(DevToolsCrashAnalysisModule STATIC)

devtools_target_options(DevToolsCrashAnalysisModule)
target_link_libraries(DevToolsCrashAnalysisModule
        PUBLIC
        ddModule
        PRIVATE
        ddCommon
        dd_tools
        ddModuleUtil
        MercuryModuleExt
        DevToolsCommonStatic
        QtCustomWidgets
        QtUtils
        Qt::Widgets
        LibDevTrace
)

set_target_properties(DevToolsCrashAnalysisModule
        PROPERTIES
        FOLDER "DevTools Modules/Gui"
)

target_include_directories(DevToolsCrashAnalysisModule
        PRIVATE
        src
        ${EXTERNAL_DIR}
        ${LIBDEVTRACE_DIR}
        PUBLIC
        inc
)

target_sources(DevToolsCrashAnalysisModule PRIVATE
        src/gui/crash_analysis_prelaunch_settings_helper.cpp
        src/gui/crash_analysis_view.cpp
        src/gui/crash_analysis_error_dialog.cpp
        src/gui/crash_analysis_userdata_view.cpp
        src/gui/crash_analysis_userdata_view_model.cpp
        src/gui/crash_analysis_view_model.cpp
        src/gui/crash_analysis_view_model.cpp
        src/summary/qt_rgd_summary_generator.cpp
        src/CrashAnalysis.cpp
        PUBLIC
        inc/CrashAnalysis.h
)
