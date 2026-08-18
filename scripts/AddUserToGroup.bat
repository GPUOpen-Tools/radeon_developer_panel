@echo off

REM Copyright (c) 2017-2025 Advanced Micro Devices, Inc. All rights reserved.

REM The steps in this script are required in order to get a full set of data
REM to appear in the System Activity view in Radeon GPU Profiler for D3D12
REM applications. This script must be run with admin privileges.

REM It performs two actions:
REM
REM    1: It adds the current user to the "Performance Log Users" group.
REM    2: It attempts to add a required registry key.

REM Resolve the localized name of "Performance Log Users" (SID: S-1-5-32-559)
REM so this script works on non-English Windows installations.
set "PERF_LOG_GROUP="
for /f "delims=" %%G in ('powershell -NoProfile -Command "(New-Object System.Security.Principal.SecurityIdentifier('S-1-5-32-559')).Translate([System.Security.Principal.NTAccount]).Value.Split('\')[-1]"') do set "PERF_LOG_GROUP=%%G"

if not defined PERF_LOG_GROUP (
    echo ERROR: Could not resolve the "Performance Log Users" group name for this locale.
    echo Falling back to the English group name.
    set "PERF_LOG_GROUP=Performance Log Users"
)

echo Copyright (c) 2017-2025 Advanced Micro Devices, Inc. All rights reserved.
echo.
echo This script will add the current user to the "%PERF_LOG_GROUP%" group.
echo This script will also add a required registry key.
echo Run this script with "--cleanup" to delete the registry key and remove the
echo current user from the group.
echo.
echo Domain:        %userdomain%
echo Username:      %USERNAME%
echo Computer Name: %COMPUTERNAME%
echo.

if "%1"=="--cleanup" (GOTO CLEANUP)

net session >nul 2>&1
if not %errorLevel% == 0 (
    echo Please run this script as Administrator.
    GOTO DONE
)

echo Running Script as Administrator.
echo.

echo **** Attempting to add user %USERNAME% to "%PERF_LOG_GROUP%" group ****
echo.

net localgroup "%PERF_LOG_GROUP%" | findstr /i /x /c:"%USERNAME%" >nul 2>&1
if %errorLevel% == 0 (
    echo User %USERNAME% is already added to "%PERF_LOG_GROUP%".
    echo.
) else (
    net localgroup "%PERF_LOG_GROUP%" "%USERNAME%" /add && (
        echo Please reboot your system for these changes to take effect.
        echo.
    )
)

echo **** Attempting to add ETW enablement registry key ****
echo.

"%~dp0\\EnableSyncPrimitives.exe"
if %errorLevel% == 2 (
    echo ETW enablement registry key is already added.
    echo.
) else if %errorLevel% == 1 (
    echo Error when trying to update the Windows registry.
    echo.
) else (
    echo Registry updated successfully.
    echo.
)

echo Exiting...
GOTO DONE

:CLEANUP
net session >nul 2>&1
if not %errorLevel% == 0 (
    echo Please run this script as Administrator.
    GOTO DONE
)

echo Running Script as Administrator.
echo.

echo **** Attempting to remove user %USERNAME% from "%PERF_LOG_GROUP%" group ****
echo.

net localgroup "%PERF_LOG_GROUP%" "%USERNAME%" /delete && (
    echo Please reboot your system for these changes to take effect.
    echo.
)

echo **** Attempting to remove ETW enablement registry key ****
echo.

"%~dp0\\EnableSyncPrimitives.exe" --cleanup
if %errorLevel% == 1 (
    echo Error when trying to update the Windows registry.
    echo.
) else (
    echo Registry updated successfully.
    echo.
)

echo Exiting...

:DONE
