#######################################################################################################################
### Copyright (c) 2019-2025 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

add_subdirectory(cli)

if (NOT HEADLESS)
    add_subdirectory(gui)
endif ()
