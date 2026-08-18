#######################################################################################################################
### Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(DEVTOOLS_MODULE_FOLDER "DevTools Modules/Raytracing")

add_library(DevToolsRaytracingModule STATIC)

devtools_target_options(DevToolsRaytracingModule)
target_link_libraries(DevToolsRaytracingModule
        PUBLIC
        ddModule
        PRIVATE
        ddCommon
        dd_tools
        ddModuleUtil
        MercuryModuleExt
        DevToolsCommonStatic
        amdrdf
        QtCustomWidgets
        QtUtils
        Qt::Widgets
        LibDevTrace
)

set_target_properties(DevToolsRaytracingModule
        PROPERTIES
        FOLDER "DevTools Modules/Gui"
)

target_include_directories(DevToolsRaytracingModule
        PRIVATE
        src
        ${EXTERNAL_DIR}
        ${LIBDEVTRACE_DIR}
        PUBLIC
        inc
)

target_sources(DevToolsRaytracingModule PRIVATE
        src/gui/raytracing_view.h
        src/gui/raytracing_view.cpp
        src/gui/raytracing_userdata_view.h
        src/gui/raytracing_userdata_view.cpp
        src/gui/raytracing_userdata_view_model.h
        src/gui/raytracing_userdata_view_model.cpp
        src/gui/raytracing_view_model.h
        src/gui/raytracing_view_model.cpp
        src/utility/rra_trace_validator.h
        src/utility/rra_trace_validator.cpp
        src/Raytracing.cpp
        PUBLIC
        inc/Raytracing.h
)
