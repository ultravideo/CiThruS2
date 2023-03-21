@echo off
setlocal
set ROOT_DIR=%~dp0

if "%VisualStudioVersion%" == "" (
    echo(
    echo Please run this script from x64 Native Tools Command Prompt for VS 2019 or newer.
    goto :buildfailed
)

set powershell=powershell
where powershell > nul 2>&1
if ERRORLEVEL 1 goto :nopwsh
goto :start
:nopwsh
echo PowerShell is required by this script: please install it.
goto :buildfailed

:start
chdir /d %ROOT_DIR%
IF NOT EXIST temp mkdir temp

echo Downloading AirSim...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://github.com/microsoft/AirSim/archive/v1.7.0-windows.zip', 'temp\AirSim.zip')"
echo Extracting AirSim...
%powershell% -command "Expand-Archive -Path temp\AirSim.zip -DestinationPath temp"
del temp\AirSim.zip /q

echo Running AirSim setup...
call temp\AirSim-1.7.0-windows\build.cmd

echo Copying AirSim plugin files...
IF NOT EXIST Plugins mkdir Plugins
robocopy temp\AirSim-1.7.0-windows\Unreal\Plugins Plugins /e

echo Cleaning up AirSim files...
rmdir temp\AirSim-1.7.0-windows /s /q

echo AirSim successfully set up.

echo Downloading CiThruS2 content...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://ultravideo.fi/sim_env/cithrus2_sim_env_content_06_03_2023.zip', 'temp\CiThruS2_content.zip')"
echo Extracting CiThruS2 content...
%powershell% -command "Expand-Archive -Path temp\CiThruS2_content.zip -DestinationPath . -Force"
del temp\CiThruS2_content.zip /q

echo CiThruS2 content successfully set up.

rmdir temp

:buildfailed
chdir /d %ROOT_DIR% 
exit /b 1