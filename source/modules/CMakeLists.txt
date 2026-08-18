#######################################################################################################################
### Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

# Add current directory to include path to support #include <common/inc/*.h>
include_directories(".")

### Common Shared Code ################################################################################################
add_subdirectory(common)

### Device Clocks Module ##############################################################################################
add_subdirectory(deviceclocks)

### Profiling Module ##################################################################################################
add_subdirectory(profiling)

### Raytracing Module #################################################################################################
add_subdirectory(raytracing)

### Memory Trace Module ###############################################################################################
add_subdirectory(memorytrace)

### Crash Analysis Module ###############################################################################################
add_subdirectory(crashanalysis)

