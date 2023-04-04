@echo off
setlocal
set AIRSIM_VER=1.7.0
set DLSS_VER=20220623-dlss-plugin-ue426
set KVAZAAR_VER=2.2.0
set YASM_VER=1.3.0
set UVGRTP_VER=2.3.0
set CITHRUS_CONTENT_VER=06_03_2023

set ROOT_DIR=%~dp0

if "%VisualStudioVersion%" == "" (
    echo(
    echo Please run this script from x64 Native Tools Command Prompt for VS 2019 or newer.
    goto :setupfailed
)

:: Get PowerShell
set powershell=powershell
where powershell > nul 2>&1
if ERRORLEVEL 1 (
	echo PowerShell is required by this script: please install it.
	goto :setupfailed
)

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
call temp\AirSim-%AIRSIM_VER%-windows\build.cmd --Release || goto :airsimbuildfailed

echo Copying AirSim plugin files...
IF NOT EXIST Plugins mkdir Plugins
robocopy temp\AirSim-%AIRSIM_VER%-windows\Unreal\Plugins Plugins /e

:: Tiny patch to prevent AirSim from changing the Unreal Engine world origin, which would break CiThruS traffic systems
%powershell% -command "((Get-Content -path Plugins\AirSim\Source\SimMode\SimModeBase.cpp -Raw) -replace [regex]::Escape('    this->GetWorld()->SetNewWorldOrigin(FIntVector(player_loc) + this->GetWorld()->OriginLocation);'),'    //this->GetWorld()->SetNewWorldOrigin(FIntVector(player_loc) + this->GetWorld()->OriginLocation);') | Set-Content -Path Plugins\AirSim\Source\SimMode\SimModeBase.cpp"

echo Cleaning up AirSim files...
rmdir temp\AirSim-%AIRSIM_VER%-windows /s /q

echo Finished setting up AirSim.

goto :dlsssetup

:airsimdownloadfailed
echo Failed to download AirSim!
goto :setupfailed

:airsimbuildfailed
echo Failed to build AirSim!
goto :setupfailed

:dlsssetup
echo Downloading Nvidia DLSS...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://developer.nvidia.com/%DLSS_VER%zip', 'temp\DLSS.zip')" || goto :dlssdownloadfailed
echo Extracting Nvidia DLSS...
mkdir temp\DLSS
%powershell% -command "Expand-Archive -Path temp\DLSS.zip -DestinationPath temp\DLSS"
del temp\DLSS.zip /q

mkdir Plugins\DLSS
robocopy temp\DLSS\DLSS Plugins\DLSS /e

rmdir temp\DLSS /s /q

echo Nvidia DLSS successfully set up.

goto :fsrsetup

:dlssdownloadfailed
echo Failed to download Nvidia DLSS!
goto :setupfailed

:fsrsetup
echo Downloading AMD FSR 2...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://gpuopen.com/download-Unreal-Engine-FSR2-plugin/', 'temp\FSR.zip')" || goto :fsrdownloadfailed
echo Extracting AMD FSR 2...
%powershell% -command "Expand-Archive -Path temp\FSR.zip -DestinationPath temp"
del temp\FSR.zip /q

mkdir Plugins\FSR2
robocopy "temp\FSR2-UE-2-2\FSR2 2.2\FSR2-426\FSR2" Plugins\FSR2 /e

rmdir temp\FSR2-UE-2-2 /s /q

echo AMD FSR 2 successfully set up.

goto :kvazaarsetup

:fsrdownloadfailed
echo Failed to download FSR 2!
goto :setupfailed

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
goto :setupfailed

:yasmdownloadfailed
echo Failed to download Yasm!
goto :setupfailed

:kvazaarbuildfailed
echo Failed to build Kvazaar!
goto :setupfailed

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
goto :setupfailed

:uvgrtpbuildfailed
echo Failed to build uvgRTP!
goto :setupfailed

:contentsetup
echo Downloading CiThruS2 content...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://ultravideo.fi/sim_env/cithrus2_sim_env_content_%CITHRUS_CONTENT_VER%.zip', 'temp\CiThruS2_content.zip')" || goto :contentdownloadfailed
echo Extracting CiThruS2 content...
%powershell% -command "Expand-Archive -Path temp\CiThruS2_content.zip -DestinationPath . -Force"
del temp\CiThruS2_content.zip /q

echo Finished setting up CiThruS2 content.

echo CiThruS2 setup was successful! Next, open HervantaUE4.uproject in Unreal Engine 4 to access the simulation environment.

IF EXIST temp rmdir temp /s /q
chdir /d %ROOT_DIR% 
exit /b 0

:contentdownloadfailed
echo Failed to download uvgRTP!
goto :setupfailed

:setupfailed
IF EXIST temp rmdir temp /s /q
chdir /d %ROOT_DIR% 
exit /b 1