@echo off
setlocal
set AIRSIM_VER=1.7.0
set KVAZAAR_VER=2.2.0
set YASM_VER=1.3.0
set UVGRTP_VER=2.3.0
set CITHRUS_CONTENT_VER=06_03_2023

set ROOT_DIR=%~dp0

if "%VisualStudioVersion%" == "" (
    echo(
    echo Please run this script from x64 Native Tools Command Prompt for VS 2019 or newer.
    goto :finishsetup
)

:: Get PowerShell
set powershell=powershell
where powershell > nul 2>&1
if ERRORLEVEL 1 goto :powershellfailed
goto :start

:powershellfailed
echo PowerShell is required by this script: please install it.
goto :finishsetup

:start
chdir /d %ROOT_DIR%
IF NOT EXIST temp mkdir temp

echo Setting up CiThruS2...

:: AirSim setup
echo Downloading AirSim...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://github.com/microsoft/AirSim/archive/v%AIRSIM_VER%-windows.zip', 'temp\AirSim.zip')" || goto :airsimdownloadfailed
echo Extracting AirSim...
%powershell% -command "Expand-Archive -Path temp\AirSim.zip -DestinationPath temp"
del temp\AirSim.zip /q

echo Running AirSim setup...
call temp\AirSim-%AIRSIM_VER%-windows\build.cmd || goto :airsimbuildfailed

echo Copying AirSim plugin files...
IF NOT EXIST Plugins mkdir Plugins
robocopy temp\AirSim-%AIRSIM_VER%-windows\Unreal\Plugins Plugins /e

echo Cleaning up AirSim files...
rmdir temp\AirSim-%AIRSIM_VER%-windows /s /q

echo Finished setting up AirSim.

goto :kvazaarsetup

:airsimdownloadfailed
echo Failed to download AirSim!
goto :finishsetup

:airsimbuildfailed
echo Failed to build AirSim!
goto :finishsetup

:kvazaarsetup
echo Downloading Kvazaar...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://github.com/ultravideo/kvazaar/archive/v%KVAZAAR_VER%.zip', 'temp\Kvazaar.zip')" || goto :kvazaardownloadfailed
echo Extracting Kvazaar...
%powershell% -command "Expand-Archive -Path temp\Kvazaar.zip -DestinationPath temp -Force"
del temp\Kvazaar.zip /q

:: Yasm is needed to build Kvazaar
echo Downloading Yasm...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('http://www.tortall.net/projects/yasm/releases/vsyasm-%YASM_VER%-win64.zip', 'temp\Yasm.zip')" || goto :yasmdownloadfailed
echo Extracting Yasm...
%powershell% -command "Expand-Archive -Path temp\Yasm.zip -DestinationPath temp\Yasm -Force"
del temp\Yasm.zip /q

:: Kvazaar finds Yasm through the PATH environment variable
set PATH=%PATH%;%ROOT_DIR%temp\Yasm\

echo Building Kvazaar...
IF NOT EXIST ThirdParty\Kvazaar\Lib mkdir ThirdParty\Kvazaar\Lib
IF NOT EXIST ThirdParty\Kvazaar\Include mkdir ThirdParty\Kvazaar\Include

:: Change the build configuration of Kvazaar to disable assembly output: otherwise Kvazaar doesn't work with CiThruS
%powershell% -command "((Get-Content -path temp\kvazaar-%KVAZAAR_VER%\build\C_Properties.props -Raw) -replace 'AssemblyAndSourceCode','NoListing') | Set-Content -Path temp\kvazaar-%KVAZAAR_VER%\build\C_Properties.props"

msbuild temp\kvazaar-%KVAZAAR_VER%\build\kvazaar_VS2015.sln /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v142 /p:WindowsTargetPlatformVersion=10.0 || goto :kvazaarbuildfailed

robocopy temp\kvazaar-%KVAZAAR_VER%\build\x64-Release-libs ThirdParty\Kvazaar\Lib /e
robocopy temp\kvazaar-%KVAZAAR_VER%\src ThirdParty\Kvazaar\Include kvazaar.h

echo Cleaning up Kvazaar and Yasm files...
rmdir temp\Yasm /s /q
rmdir temp\kvazaar-%KVAZAAR_VER% /s /q

echo Kvazaar successfully set up.

goto :uvgrtpsetup

:kvazaardownloadfailed
echo Failed to download Kvazaar!
goto :finishsetup

:yasmdownloadfailed
echo Failed to download Yasm!
goto :finishsetup

:kvazaarbuildfailed
echo Failed to build Kvazaar!
goto :finishsetup

:uvgrtpsetup
echo Downloading uvgRTP...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://github.com/ultravideo/uvgRTP/archive/v%UVGRTP_VER%.zip', 'temp\uvgRTP.zip')" || goto :uvgrtpdownloadfailed
echo Extracting uvgRTP...
%powershell% -command "Expand-Archive -Path temp\uvgRTP.zip -DestinationPath temp -Force"
del temp\uvgRTP.zip /q

echo Building uvgRTP...
IF NOT EXIST ThirdParty\uvgRTP\Lib mkdir ThirdParty\uvgRTP\Lib
IF NOT EXIST ThirdParty\uvgRTP\Include mkdir ThirdParty\uvgRTP\Include

mkdir temp\uvgRTP-%UVGRTP_VER%\build
cmake temp\uvgRTP-%UVGRTP_VER% -Btemp\uvgRTP-%UVGRTP_VER%\build

msbuild temp\uvgRTP-%UVGRTP_VER%\build\uvgrtp.sln /p:Configuration="Release" /p:Platform=x64 /p:PlatformToolset=v142 /p:WindowsTargetPlatformVersion=10.0 || goto :uvgrtpbuildfailed

robocopy temp\uvgRTP-%UVGRTP_VER%\build\Release ThirdParty\uvgRTP\Lib /e
robocopy temp\uvgRTP-%UVGRTP_VER%\include\uvgrtp ThirdParty\uvgRTP\Include

echo Cleaning up uvgRTP files...
rmdir temp\uvgRTP-%UVGRTP_VER% /s /q

echo Finished setting up uvgRTP.

goto :contentsetup

:uvgrtpdownloadfailed
echo Failed to download uvgRTP!
goto :finishsetup

:uvgrtpbuildfailed
echo Failed to build uvgRTP!
goto :finishsetup

:contentsetup
echo Downloading CiThruS2 content...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://ultravideo.fi/sim_env/cithrus2_sim_env_content_%CITHRUS_CONTENT_VER%.zip', 'temp\CiThruS2_content.zip')" || goto :contentdownloadfailed
echo Extracting CiThruS2 content...
%powershell% -command "Expand-Archive -Path temp\CiThruS2_content.zip -DestinationPath . -Force"
del temp\CiThruS2_content.zip /q

echo Finished setting up CiThruS2 content.

echo CiThruS2 setup was successful! Next, open HervantaUE4.uproject in Unreal Engine 4 to access the simulation environment.

goto :finishsetup

:contentdownloadfailed
echo Failed to download uvgRTP!
goto :finishsetup

:finishsetup
IF EXIST temp rmdir temp /s /q
chdir /d %ROOT_DIR% 
exit /b 1