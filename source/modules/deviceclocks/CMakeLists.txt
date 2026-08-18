#######################################################################################################################
### Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

set(DEVTOOLS_MODULE_FOLDER "DevTools Modules/Device Clocks")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

### Module ############################################################################################################
add_library(DevToolsDeviceClocksModule STATIC)
devtools_target_options(DevToolsDeviceClocksModule)
target_link_libraries(DevToolsDeviceClocksModule
        PUBLIC
        ddModule
        PRIVATE
        dd_tools
        ddCommon
        ddModuleUtil
        MercuryModuleExt
        system_info
        DevToolsCommonStatic
        QtCustomWidgets
        QtUtils
        LibDevTrace
        Qt::Widgets
)

target_include_directories(DevToolsDeviceClocksModule
        PRIVATE
        src
        ${EXTERNAL_DIR}
        ${LIBDEVTRACE_DIR}
        PUBLIC
        inc
)

target_sources(DevToolsDeviceClocksModule PRIVATE
        src/gui/clock_mode_model.h
        src/gui/clock_mode_model.cpp
        src/gui/clock_mode_widget.h
        src/gui/clock_mode_widget.cpp
        src/gui/clock_mode_widget.ui
        src/gui/device_clocks_gpu_model.h
        src/gui/device_clocks_gpu_model.cpp
        src/gui/device_clocks_view.h
        src/gui/device_clocks_view.cpp
        src/gui/device_clocks_model.h
        src/gui/device_clocks_model.cpp
        src/gui/device_clocks_view.ui
        src/DeviceClocks.cpp
        PUBLIC
        inc/DeviceClocks.h
)
