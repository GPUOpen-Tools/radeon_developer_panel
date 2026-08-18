#######################################################################################################################
### Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(DEVTOOLS_MODULE_FOLDER "DevTools Modules/Memory Trace")

### Module Core #######################################################################################################
add_library(DevToolsMemoryTraceModule STATIC)
devtools_target_options(DevToolsMemoryTraceModule)
target_link_libraries(DevToolsMemoryTraceModule
        PUBLIC
        ddModule
        PRIVATE
        ddCommon
        dd_tools
        ddModuleUtil
        QtUtils
        QtCustomWidgets
        MercuryModuleExt
        DevToolsCommonStatic
        Qt::Widgets
        LibDevTrace
)
set_target_properties(DevToolsMemoryTraceModule
        PROPERTIES
        FOLDER "DevTools Modules/Gui"
)

target_include_directories(DevToolsMemoryTraceModule
        PRIVATE
        src
        ${EXTERNAL_DIR}
        ${LIBDEVTRACE_DIR}
        PUBLIC
        inc
)
target_include_directories(DevToolsMemoryTraceModule SYSTEM PRIVATE
        ${JSON_LIB_DIR}
)

target_sources(DevToolsMemoryTraceModule
        PRIVATE
        src/gui/memory_trace_module_definitions.h
        src/gui/memory_trace_view.h
        src/gui/memory_trace_view.cpp
        src/gui/memory_trace_view_model.h
        src/gui/memory_trace_view_model.cpp
        src/gui/memory_trace_userdata_view_model.h
        src/gui/memory_trace_userdata_view_model.cpp
        src/MemoryTrace.cpp
        PUBLIC
        inc/MemoryTrace.h
)
