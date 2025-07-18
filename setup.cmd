@echo off
setlocal
set AIRSIM_VER=2.1.0
set DLSS_VER=ue5.4_dlss_3.7.20_plugin_2024.09.06
set FSR_VER=520
set KVAZAAR_VER=2.3.1
set YASM_VER=1.3.0
set UVGRTP_VER=3.1.6
set OPENHEVC_VER=ffmpeg_update
set CITHRUS_CONTENT_VER=21_01_2025

set ROOT_DIR=%~dp0

:: Get PowerShell
set powershell=powershell
where powershell > nul 2>&1
if errorlevel 1 (
	echo PowerShell is required by this script: please install it.
	goto :setupfailed
)

:: Get VS Command Prompt
if "%VisualStudioVersion%" == "" (
	echo Starting x64 Native Tools Command Prompt for VS 2022...
	:: Try the default installation locations
    call "%ProgramW6432%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
	if errorlevel 1 call "%ProgramW6432%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
	if errorlevel 1 call "%ProgramW6432%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
	if errorlevel 1 call "%ProgramW6432%\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
	if errorlevel 1 goto :vslocatefailed
)

goto :setupstart

:vslocatefailed
echo Could not find x64 Native Tools Command Prompt for VS 2022 automatically: Please open it manually and run this script in it.
goto :setupfailed

:setupstart
chdir /d %ROOT_DIR%
if exist temp rmdir temp /s /q
mkdir temp

echo Setting up CiThruS2...

:: AirSim is not available for UE 5.4, skip for now
goto :dlsssetup

:: AirSim setup
if exist Plugins\AirSim (
	echo AirSim already found at Plugins\AirSim, skipping... ^(Please manually delete the folder if you'd like to reinstall it.^)
	goto :dlsssetup
)

echo Downloading AirSim (from Colosseum)...
git clone https://github.com/ArttuLeppaaho/Colosseum temp\Colosseum

cd temp\Colosseum
git submodule update --init
cd ..\..

echo Running AirSim setup...
call temp\Colosseum\build.cmd --Release || goto :airsimbuildfailed

echo Copying AirSim plugin files...
if not exist Plugins mkdir Plugins

robocopy temp\Colosseum\Unreal\Plugins Plugins /e

:: Tiny patch to prevent AirSim from changing the Unreal Engine world origin, which would break CiThruS traffic systems
%powershell% -command "((Get-Content -path Plugins\AirSim\Source\SimMode\SimModeBase.cpp -Raw) -replace [regex]::Escape('    this->GetWorld()->SetNewWorldOrigin(FIntVector(player_loc) + this->GetWorld()->OriginLocation);'),'    //this->GetWorld()->SetNewWorldOrigin(FIntVector(player_loc) + this->GetWorld()->OriginLocation);') | Set-Content -Path Plugins\AirSim\Source\SimMode\SimModeBase.cpp"

:: Another patch to remove an unused folder which was accidentally included in the Colosseum release and prevents it from compiling
if exist Plugins\AirSim\Source\AssetCode rmdir Plugins\AirSim\Source\AssetCode /s /q

echo Cleaning up AirSim files...
rmdir temp\Colosseum-%AIRSIM_VER% /s /q

echo AirSim successfully set up.

goto :dlsssetup

:airsimdownloadfailed
echo Failed to download AirSim!
goto :setupfailed

:airsimbuildfailed
echo Failed to build AirSim!
goto :setupfailed

:dlsssetup
if exist Plugins\DLSS (
	echo Nvidia DLSS already found at Plugins\DLSS, skipping... ^(Please delete the folder manually if you'd like to reinstall it.^)
	goto :fsrsetup
)

echo Downloading Nvidia DLSS...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://developer.nvidia.com/downloads/assets/gameworks/downloads/secure/dlss/%DLSS_VER%.zip', 'temp\DLSS.zip')" || goto :dlssdownloadfailed
echo Extracting Nvidia DLSS...
mkdir temp\DLSS
%powershell% -command "Expand-Archive -Path temp\DLSS.zip -DestinationPath temp\DLSS"
del temp\DLSS.zip /q

mkdir Plugins\DLSS

robocopy temp\DLSS\Plugins\DLSS Plugins\DLSS /e

rmdir temp\DLSS /s /q

echo Nvidia DLSS successfully set up.

goto :fsrsetup

:dlssdownloadfailed
echo Failed to download Nvidia DLSS!
goto :setupfailed

:fsrsetup
:: FSR is not available for UE 5.4, skip for now
goto :impostorbakersetup

if exist Plugins\FSR2 IF EXIST Plugins\FSR2MovieRenderPipeline (
	echo AMD FSR2 already found at Plugins\FSR2 and Plugins\FSR2MovieRenderPipeline, skipping... ^(Please delete the folders manually if you'd like to reinstall it.^)
	goto :impostorbakersetup
)

echo Downloading AMD FSR 2...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://gpuopen.com/download-Unreal-Engine-FSR2-plugin/', 'temp\FSR.zip')" || goto :fsrdownloadfailed
echo Extracting AMD FSR 2...
%powershell% -command "Expand-Archive -Path temp\FSR.zip -DestinationPath temp"
del temp\FSR.zip /q

:: If there are existing plugin files, remove them to avoid conflicts
if exist Plugins\FSR2 rmdir Plugins\FSR2 /s /q
if exist Plugins\FSR2MovieRenderPipeline rmdir Plugins\FSR2MovieRenderPipeline /s /q

mkdir Plugins\FSR2
mkdir Plugins\FSR2MovieRenderPipeline

for /d %%i in (temp\FSR2*) do set "FIRST_FSR_FOLDER_NAME=%%i"
for /d %%i in (%FIRST_FSR_FOLDER_NAME%\FSR2*) do set "SECOND_FSR_FOLDER_NAME=%%i"

robocopy "%SECOND_FSR_FOLDER_NAME%\FSR2-%FSR_VER%\FSR2" Plugins\FSR2 /e
robocopy "%SECOND_FSR_FOLDER_NAME%\FSR2-%FSR_VER%\FSR2MovieRenderPipeline" Plugins\FSR2MovieRenderPipeline /e

rmdir %FIRST_FSR_FOLDER_NAME% /s /q

echo AMD FSR 2 successfully set up.

goto :impostorbakersetup

:fsrdownloadfailed
echo Failed to download FSR 2!
goto :setupfailed

:impostorbakersetup
if exist Plugins\ImpostorBaker-master (
	echo ImpostorBaker already found at Plugins\ImpostorBaker-master, skipping... ^(Please delete the folder manually if you'd like to reinstall it.^)
	goto :librarysetup
)

echo Downloading ImpostorBaker...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://github.com/ictusbrucks/ImpostorBaker/archive/refs/heads/master.zip', 'temp\impostorbaker.zip')" || goto :impostorbakerdownloadfailed
echo Extracting ImpostorBaker...
%powershell% -command "Expand-Archive -Path temp\ImpostorBaker.zip -DestinationPath Plugins"
del temp\ImpostorBaker.zip /q

echo ImpostorBaker successfully set up.

goto :librarysetup

:impostorbakerdownloadfailed
echo Failed to download ImpostorBaker!
goto :setupfailed

:librarysetup
:: Get the compiler version and paste it in Unreal Engine's build configuration to make sure the libraries and CiThruS are compiled with the same compiler. Otherwise they will fail to link
for /f "delims=" %%i in ('cl 2^>^&1 ^| findstr "[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*"') do set COMPILER_VER=%%i
for /f "delims=" %%i in ('%powershell% -command "('%COMPILER_VER%' | Select-String -Pattern '\.[0-9][0-9]*\.[0-9][0-9]*').Matches.Value"') do set COMPILER_VER=%%i

if not exist Saved mkdir Saved
if not exist Saved\UnrealBuildTool mkdir Saved\UnrealBuildTool
if not exist Saved\UnrealBuildTool\BuildConfiguration.xml (
	(
		echo ^<^?xml version="1.0" encoding="utf-8" ^?^>
		echo ^<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration"^>
		echo ^	^<WindowsPlatform^>
		echo ^		^<CompilerVersion^>14.00.00000^</CompilerVersion^>
		echo ^		^<Compiler^>VisualStudio2022^</Compiler^>
		echo ^	^</WindowsPlatform^>
		echo ^	^<VCProjectFileGenerator^>
		echo ^		^<Version^>VisualStudio2022^</Version^>
		echo ^	^</VCProjectFileGenerator^>
		echo ^</Configuration^>
	) > Saved\UnrealBuildTool\BuildConfiguration.xml
)

%powershell% -command "((Get-Content -path Saved/UnrealBuildTool/BuildConfiguration.xml -Raw) -replace '14\.[0-9]+\.[0-9]+','14%COMPILER_VER%') | Set-Content -Path Saved/UnrealBuildTool/BuildConfiguration.xml"

if not exist ThirdParty\Kvazaar goto :yasmsetup
if not exist ThirdParty\OpenHEVC goto :yasmsetup

goto :uvgrtpsetup

:yasmsetup
:: Yasm is needed to build Kvazaar and OpenHEVC
echo Downloading Yasm...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('http://www.tortall.net/projects/yasm/releases/vsyasm-%YASM_VER%-win64.zip', 'temp\Yasm.zip')" || goto :yasmdownloadfailed
echo Extracting Yasm...
%powershell% -command "Expand-Archive -Path temp\Yasm.zip -DestinationPath temp\Yasm -Force"
del temp\Yasm.zip /q

:: Kvazaar finds Yasm through the PATH environment variable
set PATH=%PATH%;%ROOT_DIR%temp\Yasm\

goto :kvazaarsetup

:yasmdownloadfailed
echo Failed to download Yasm!
goto :setupfailed

:kvazaarsetup
if exist ThirdParty\Kvazaar (
	echo Kvazaar already found at ThirdParty\Kvazaar, skipping... ^(Please delete the folder manually if you'd like to reinstall it.^)
	goto :openhevcsetup
)

echo Downloading Kvazaar...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://github.com/ultravideo/kvazaar/archive/v%KVAZAAR_VER%.zip', 'temp\Kvazaar.zip')" || goto :kvazaardownloadfailed
echo Extracting Kvazaar...
%powershell% -command "Expand-Archive -Path temp\Kvazaar.zip -DestinationPath temp -Force"
del temp\Kvazaar.zip /q

echo Building Kvazaar...

:: Change the build configuration of Kvazaar to disable assembly output: otherwise Kvazaar doesn't work with CiThruS
%powershell% -command "((Get-Content -path temp\kvazaar-%KVAZAAR_VER%\build\C_Properties.props -Raw) -replace 'AssemblyAndSourceCode','NoListing') | Set-Content -Path temp\kvazaar-%KVAZAAR_VER%\build\C_Properties.props"

msbuild temp\kvazaar-%KVAZAAR_VER%\build\kvazaar_VS2015.sln /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 || goto :kvazaarbuildfailed

mkdir ThirdParty\Kvazaar\Lib
mkdir ThirdParty\Kvazaar\Include

robocopy temp\kvazaar-%KVAZAAR_VER% ThirdParty\Kvazaar LICENSE
robocopy temp\kvazaar-%KVAZAAR_VER%\build\x64-Release-libs ThirdParty\Kvazaar\Lib /e
robocopy temp\kvazaar-%KVAZAAR_VER%\src ThirdParty\Kvazaar\Include kvazaar.h

echo Cleaning up Kvazaar files...
rmdir temp\kvazaar-%KVAZAAR_VER% /s /q

echo Kvazaar successfully set up.

goto :openhevcsetup

:kvazaardownloadfailed
echo Failed to download Kvazaar!
goto :setupfailed

:kvazaarbuildfailed
echo Failed to build Kvazaar!
goto :setupfailed

:openhevcsetup
if exist ThirdParty\OpenHEVC (
	echo OpenHEVC already found at ThirdParty\OpenHEVC, skipping... ^(Please delete the folder manually if you'd like to reinstall it.^)
	goto :uvgrtpsetup
)

echo Downloading OpenHEVC...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://github.com/OpenHEVC/openHEVC/archive/refs/heads/%OPENHEVC_VER%.zip', 'temp\OpenHEVC.zip')" || goto :openhevcdownloadfailed
echo Extracting OpenHEVC...
%powershell% -command "Expand-Archive -Path temp\OpenHEVC.zip -DestinationPath temp -Force"
del temp\OpenHEVC.zip /q

:: Building OpenHEVC is broken on Windows, needs to be patched
echo Patching OpenHEVC...
:: Set correct CMake version and enable C11
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\CMakeLists.txt -Raw) -replace 'cmake_minimum_required \(VERSION 2\.8\)',\""cmake_minimum_required (VERSION 3.5)`nset (CMAKE_C_STANDARD 11)\"") | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\CMakeLists.txt"
:: m library doesn't exist on Windows and isn't needed anyway. Replace with explicitly enabling C11 atomics
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\CMakeLists.txt -Raw) -replace 'target_link_libraries\(LibOpenHevcWrapper m\)',\""if (MSVC)`n`ttarget_compile_options(LibOpenHevcWrapper PRIVATE /experimental:c11atomics)`nendif()\"") | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\CMakeLists.txt"
:: These definitions break MSVC and aren't needed anyway
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\CMakeLists.txt -Raw) -replace 'if\(WIN32\)\s*add_definitions\([\s\S]*?\)\s*endif\(\)','') | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\CMakeLists.txt"
:: Yasm output file extension is incorrect
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\CMakeLists.txt -Raw) -replace 'set\(YASM_OBJ \""\${CMAKE_CURRENT_BINARY_DIR}\/\${BASENAME}.o\""\)','set(YASM_OBJ \""${CMAKE_CURRENT_BINARY_DIR}/${BASENAME}.obj\"")') | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\CMakeLists.txt"
:: Inline assembly is not supported on Windows, clear this file to disable it
%powershell% -command "'' | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\libavutil\x86\intreadwrite.h"
:: Many parts of config.h have been written incorrectly for Windows and the configure script that generates it doesn't work either
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in -Raw) -replace '#define HAVE_W32THREADS 0\s*#define HAVE_OS2THREADS 0\s*#endif\s*#define HAVE_ATOMICS_GCC 1\s*#define HAVE_ATOMICS_SUNCC 0\s*#define HAVE_ATOMICS_WIN32 0',\""#define HAVE_W32THREADS 0`n#define HAVE_OS2THREADS 0`n#define HAVE_ATOMICS_GCC 1`n#define HAVE_ATOMICS_SUNCC 0`n#define HAVE_ATOMICS_WIN32 0`n#endif\"") | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in"
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in -Raw) -replace '#define HAVE_FCNTL @FCNTL_H_FOUND@','#define HAVE_FCNTL 0') | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in"
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in -Raw) -replace '#define HAVE_LSTAT 1','#define HAVE_LSTAT 0') | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in"
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in -Raw) -replace '#define HAVE_SYS_PARAM_H 1','#define HAVE_SYS_PARAM_H 0') | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in"
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in -Raw) -replace '#define HAVE_SYSCTL 1','#define HAVE_SYSCTL 0') | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in"
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in -Raw) -replace '#define HAVE_MMAP 1','#define HAVE_MMAP 0') | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in"
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in -Raw) -replace '#define HAVE_DIRENT_H 1','#define HAVE_DIRENT_H 0') | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in"
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in -Raw) -replace '#define HAVE_NANOSLEEP 1','#define HAVE_NANOSLEEP 0') | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in"
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in -Raw) -replace '#define HAVE_POSIX_MEMALIGN 1','#define HAVE_POSIX_MEMALIGN 0') | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in"
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in -Raw) -replace '#define HAVE_MEMALIGN 1','#define HAVE_MEMALIGN 0') | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in"
:: POSIX threads don't exist on Windows but there's a wrapper for them in the files
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\gpac\modules\openhevc_dec\openHevcWrapper.c -Raw) -replace '#include \""pthread.h\""','#include \""compat/w32pthreads.h\""') | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\gpac\modules\openhevc_dec\openHevcWrapper.c"
:: ATOMIC_VAR_INIT is deprecated
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\libavutil\cpu.c -Raw) -replace 'static atomic_int cpu_flags = ATOMIC_VAR_INIT\(-1\);','static atomic_int cpu_flags = -1;') | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\libavutil\cpu.c"

echo Building OpenHEVC...

cmake temp\openHEVC-%OPENHEVC_VER% -Btemp\openHEVC-%OPENHEVC_VER%\build -DYASM_EXECUTABLE="%ROOT_DIR%temp\Yasm\vsyasm.exe" -DYASM_FOUND=True -DENABLE_STATIC=True
msbuild temp\openHEVC-%OPENHEVC_VER%\build\openHEVC.sln /target:LibOpenHevcWrapper /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 || goto :openhevcbuildfailed

mkdir ThirdParty\OpenHEVC\Lib
mkdir ThirdParty\OpenHEVC\Include

robocopy temp\openHEVC-%OPENHEVC_VER% ThirdParty\OpenHEVC COPYING.LGPLv2.1
robocopy temp\openHEVC-%OPENHEVC_VER%\build\Release ThirdParty\OpenHEVC\Lib /e
robocopy temp\openHEVC-%OPENHEVC_VER%\gpac\modules\openhevc_dec ThirdParty\OpenHEVC\Include openHevcWrapper.h

echo Cleaning up OpenHEVC files...
rmdir temp\openHEVC-%OPENHEVC_VER% /s /q

echo OpenHEVC successfully set up.

goto :uvgrtpsetup

:openhevcdownloadfailed
echo Failed to download OpenHEVC!
goto :setupfailed

:openhevcbuildfailed
echo Failed to build OpenHEVC!
goto :setupfailed

:uvgrtpsetup
if exist ThirdParty\uvgRTP (
	echo uvgRTP already found at ThirdParty\uvgRTP, skipping... ^(Please delete the folder manually if you'd like to reinstall it.^)
	goto :contentsetup
)

echo Downloading uvgRTP...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://github.com/ultravideo/uvgRTP/archive/v%UVGRTP_VER%.zip', 'temp\uvgRTP.zip')" || goto :uvgrtpdownloadfailed
echo Extracting uvgRTP...
%powershell% -command "Expand-Archive -Path temp\uvgRTP.zip -DestinationPath temp -Force"
del temp\uvgRTP.zip /q

echo Building uvgRTP...

mkdir temp\uvgRTP-%UVGRTP_VER%\build
cmake temp\uvgRTP-%UVGRTP_VER% -Btemp\uvgRTP-%UVGRTP_VER%\build

msbuild temp\uvgRTP-%UVGRTP_VER%\build\uvgrtp.sln /p:Configuration="Release" /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 || goto :uvgrtpbuildfailed

mkdir ThirdParty\uvgRTP\Lib
mkdir ThirdParty\uvgRTP\Include

robocopy temp\uvgRTP-%UVGRTP_VER% ThirdParty\uvgRTP COPYING
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
if exist Content (
	echo CiThruS content already found at Content, skipping... ^(Please delete the folder manually if you'd like to redownload it.^)
	goto :setupsuccess
)

echo Downloading CiThruS2 content...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://ultravideo.fi/sim_env/cithrus2_sim_env_content_%CITHRUS_CONTENT_VER%.zip', 'temp\CiThruS2_content.zip')" || goto :contentdownloadfailed
echo Extracting CiThruS2 content...
%powershell% -command "Expand-Archive -Path temp\CiThruS2_content.zip -DestinationPath . -Force"
del temp\CiThruS2_content.zip /q

echo Finished setting up CiThruS2 content.
goto :setupsuccess

:contentdownloadfailed
echo Failed to download CiThruS2 content!
goto :setupfailed

:setupsuccess
if exist temp\Yasm (
	echo Cleaning up Yasm files...
	rmdir temp\Yasm /s /q
)

echo CiThruS2 setup was successful! Next, open CiThruS.uproject in Unreal Engine 5 to access the simulation environment.

if exist temp rmdir temp /s /q
chdir /d %ROOT_DIR%
pause
exit /b 0

:setupfailed
if exist temp rmdir temp /s /q
chdir /d %ROOT_DIR%
pause
exit /b 1