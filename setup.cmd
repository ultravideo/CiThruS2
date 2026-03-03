@echo off
setlocal

set AIRSIM_VER=2.1.0
set DLSS_VER=DLSS-4.5/2026.02.10_UE5.6_DLSS4.5Plugin_v8.5.0
set FSR_VER=560
set KVAZAAR_VER=2.3.2
set YASM_VER=1.3.0
set UVGRTP_VER=3.1.6
set OPENHEVC_VER=ffmpeg_update
set FPNG_VER=1.0.6
set OPENSSL_VER=3.6.1
set PAHO_C_VER=1.3.15
set PAHO_CPP_VER=1.5.3
set CITHRUS_CONTENT_VER=24_11_2025

set COLOR_FAILURE=[31m
set COLOR_WARNING=[33m
set COLOR_SUCCESS=[32m
set COLOR_SKIP=[36m
set COLOR_RESET=[0m

goto :setupstart

:downloadfailed
echo %COLOR_FAILURE%Failed to download %~1!%COLOR_RESET%
set ANY_FAILED=1

exit /b 0

:buildfailed
echo %COLOR_FAILURE%Failed to build %~1!%COLOR_RESET%
set ANY_FAILED=1

exit /b 0

:dependencymissing
if "%~1"=="" exit /b 1
if "%~2"=="" exit /b 1

if exist %~1 (
	echo %COLOR_SKIP%%~2 already found at %~1, skipping...%COLOR_RESET% ^(Please delete the folder manually if you'd like to reinstall it.^)
	exit /b 0
)
	
exit /b 1

:powershellsetup
set powershell=powershell
where powershell > nul 2>&1
if errorlevel 1 (
	echo %COLOR_FAILURE%PowerShell is required by this script; please install it.%COLOR_RESET%
	exit /b 1
)

exit /b 0

:visualstudiosetup
:: Get VS Command Prompt
if "%VisualStudioVersion%" == "" (
	echo Starting x64 Native Tools Command Prompt for Visual Studio 2022...
	:: Try the default installation locations
    call "%ProgramW6432%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
	if errorlevel 1 call "%ProgramW6432%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
	if errorlevel 1 call "%ProgramW6432%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
	if errorlevel 1 call "%ProgramW6432%\Microsoft Visual Studio\2022\Preview\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
	if errorlevel 1 (
		echo %COLOR_FAILURE%Could not find x64 Native Tools Command Prompt for Visual Studio 2022 automatically. Please open it manually and run this script in it.%COLOR_RESET%
		exit /b 1
	)
)

exit /b 0

:yasmsetup
if exist temp\Yasm (
	exit /b 0
)

:: Yasm is needed to build Kvazaar and OpenHEVC
echo Downloading Yasm...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('http://www.tortall.net/projects/yasm/releases/vsyasm-%YASM_VER%-win64.zip', 'temp\Yasm.zip')" || call :downloadfailed yasm && exit /b 1
echo Extracting Yasm...
%powershell% -command "Expand-Archive -Path temp\Yasm.zip -DestinationPath temp\Yasm -Force"
del temp\Yasm.zip /q

:: Kvazaar finds Yasm through the PATH environment variable
set PATH=%PATH%;"%ROOT_DIR%temp\Yasm\"

exit /b 0

:opensslsetup
if exist temp\OpenSSL (
	exit /b 0
)

:: OpenSSL is needed to build Eclipse Paho
echo Downloading OpenSSL...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://download.firedaemon.com/FireDaemon-OpenSSL/openssl-%OPENSSL_VER%.zip', 'temp\OpenSSL.zip')" || call :downloadfailed OpenSSL && exit /b 1
echo Extracting OpenSSL...
%powershell% -command "Expand-Archive -Path temp\OpenSSL.zip -DestinationPath temp\OpenSSL -Force"
del temp\OpenSSL.zip /q

:: CMake finds OpenSSL through this environment variable
set "OPENSSL_ROOT_DIR=%ROOT_DIR%temp\OpenSSL\x64"

exit /b 0

:contentsetup
call :dependencymissing Content "CiThruS2 content" && exit /b 0

:: Download
echo Downloading CiThruS2 content...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://ultravideo.fi/sim_env/cithrus2_sim_env_content_%CITHRUS_CONTENT_VER%.zip', 'temp\CiThruS2_content.zip')" || call :downloadfailed "CiThruS2 content" && exit /b 1
echo Extracting CiThruS2 content...
%powershell% -command "Expand-Archive -Path temp\CiThruS2_content.zip -DestinationPath . -Force"
del temp\CiThruS2_content.zip /q

:: Make editor load regions automatically
if not exist Saved mkdir Saved
if not exist Saved\Config mkdir Saved\Config
if not exist Saved\Config\WindowsEditor mkdir Saved\Config\WindowsEditor
if not exist Saved\Config\WindowsEditor\EditorPerProjectUserSettings.ini (
	(
		echo [/Script/Engine.WorldPartitionEditorPerProjectUserSettings]
		echo PerWorldEditorSettings=^(^("/Game/HervantaMapTemplate/Maps/HervantaSimulation.HervantaSimulation", ^(LoadedEditorLocationVolumes=^("LocationVolume_UAID_18C04D02B040D07301_1649670207"^)^)^)^)
	) > Saved\Config\WindowsEditor\EditorPerProjectUserSettings.ini
)

:: Finish
echo %COLOR_SUCCESS%CiThruS2 content successfully set up.%COLOR_RESET%

exit /b 0

:airsimsetup
call :dependencymissing Plugins\AirSim "AirSim" && exit /b 0

:: Download
echo Downloading AirSim (from Colosseum)...
git clone https://github.com/ArttuLeppaaho/Colosseum temp\Colosseum || call :downloadfailed AirSim && exit /b 1

cd temp\Colosseum
git submodule update --init
cd ..\..

:: Build
echo Running AirSim setup...
call temp\Colosseum\build.cmd --Release || call :buildfailed AirSim && exit /b 1

:: Copy the plugin files
echo Copying AirSim plugin files...
if not exist Plugins mkdir Plugins

robocopy temp\Colosseum\Unreal\Plugins Plugins /e

:: Tiny patch to prevent AirSim from changing the Unreal Engine world origin, which would break CiThruS traffic systems
%powershell% -command "((Get-Content -path Plugins\AirSim\Source\SimMode\SimModeBase.cpp -Raw) -replace [regex]::Escape('    this->GetWorld()->SetNewWorldOrigin(FIntVector(player_loc) + this->GetWorld()->OriginLocation);'),'    //this->GetWorld()->SetNewWorldOrigin(FIntVector(player_loc) + this->GetWorld()->OriginLocation);') | Set-Content -Path Plugins\AirSim\Source\SimMode\SimModeBase.cpp"

:: Another patch to remove an unused folder which was accidentally included in the Colosseum release and prevents it from compiling
if exist Plugins\AirSim\Source\AssetCode rmdir Plugins\AirSim\Source\AssetCode /s /q

:: Finish
echo Cleaning up AirSim files...
rmdir temp\Colosseum-%AIRSIM_VER% /s /q

echo %COLOR_SUCCESS%AirSim successfully set up.%COLOR_RESET%

exit /b 0

:dlsssetup
call :dependencymissing Plugins\DLSS "NVIDIA DLSS" && exit /b 0

:: Download
echo Downloading NVIDIA DLSS...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://developer.nvidia.com/downloads/assets/gameworks/downloads/secure/dlss/%DLSS_VER%.zip', 'temp\DLSS.zip')" || call :downloadfailed "NVIDIA DLSS" && exit /b 1
echo Extracting NVIDIA DLSS...
mkdir temp\DLSS
%powershell% -command "Expand-Archive -Path temp\DLSS.zip -DestinationPath temp\DLSS"
del temp\DLSS.zip /q

:: Copy the plugin files
mkdir Plugins\DLSS

robocopy temp\DLSS\Plugins\DLSS Plugins\DLSS /e
robocopy temp\DLSS\Plugins\StreamlineNGXCommon Plugins\StreamlineNGXCommon /e

:: Finish
echo Cleaning up DLSS files...
rmdir temp\DLSS /s /q

echo %COLOR_SUCCESS%NVIDIA DLSS successfully set up.%COLOR_RESET%

exit /b 0

:fsrsetup
call :dependencymissing Plugins\FSR "AMD FSR 4" && exit /b 0

:: Download
echo Downloading AMD FSR 4...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://gpuopen.com/download-Unreal-Engine-FSR4-plugin/', 'temp\FSR.zip')" || call :downloadfailed "AMD FSR 4" && exit /b 1
echo Extracting AMD FSR 4...
%powershell% -command "Expand-Archive -Path temp\FSR.zip -DestinationPath temp"
del temp\FSR.zip /q

:: If there are existing plugin files, remove them to avoid conflicts
if exist Plugins\FSR rmdir Plugins\FSR /s /q
if exist Plugins\FSRMovieRenderPipeline rmdir Plugins\FSRMovieRenderPipeline /s /q

:: Copy the correct plugin files (the zip file includes multiple versions)
mkdir Plugins\FSR
mkdir Plugins\FSRMovieRenderPipeline

for /d %%i in (temp\UE-FSR-*) do set "FSR_FOLDER_NAME=%%i"

robocopy "%FSR_FOLDER_NAME%\FSR-%FSR_VER%\FSR" Plugins\FSR /e
robocopy "%FSR_FOLDER_NAME%\FSR-%FSR_VER%\FSRMovieRenderPipeline" Plugins\FSRMovieRenderPipeline /e

:: Finish
echo Cleaning up FSR files...
rmdir %FSR_FOLDER_NAME% /s /q

echo %COLOR_SUCCESS%AMD FSR 4 successfully set up.%COLOR_RESET%

exit /b 0

:impostorbakersetup
call :dependencymissing Plugins\ImpostorBaker-master "ImpostorBaker" && exit /b 0

:: Download
echo Downloading ImpostorBaker...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://github.com/ictusbrucks/ImpostorBaker/archive/refs/heads/master.zip', 'temp\impostorbaker.zip')" || call :downloadfailed ImpostorBaker && exit /b 1
echo Extracting ImpostorBaker...
%powershell% -command "Expand-Archive -Path temp\ImpostorBaker.zip -DestinationPath Plugins"
del temp\ImpostorBaker.zip /q

:: Finish
echo %COLOR_SUCCESS%ImpostorBaker successfully set up.%COLOR_RESET%

exit /b 0

:kvazaarsetup
call :dependencymissing ThirdParty\Kvazaar "Kvazaar" && exit /b 0

:: Set up dependencies
call :visualstudiosetup || exit /b 1
call :yasmsetup || exit /b 1

:: Download
echo Downloading Kvazaar...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://github.com/ultravideo/kvazaar/archive/v%KVAZAAR_VER%.zip', 'temp\Kvazaar.zip')" || call :downloadfailed Kvazaar && exit /b 1
echo Extracting Kvazaar...
%powershell% -command "Expand-Archive -Path temp\Kvazaar.zip -DestinationPath temp -Force"
del temp\Kvazaar.zip /q

:: Build
echo Building Kvazaar...

:: Change the build configuration of Kvazaar to disable assembly output: otherwise Kvazaar doesn't work with CiThruS
%powershell% -command "((Get-Content -path temp\kvazaar-%KVAZAAR_VER%\build\C_Properties.props -Raw) -replace 'AssemblyAndSourceCode','NoListing') | Set-Content -Path temp\kvazaar-%KVAZAAR_VER%\build\C_Properties.props"

:: Change the build configuration of Kvazaar to disable whole program optimization. It keeps causing this issue whenever people use different VS versions https://developercommunity.visualstudio.com/t/cannot-build-after-vs-update-link-error/1348830
%powershell% -command "((Get-Content -path temp\kvazaar-%KVAZAAR_VER%\build\Release_Optimizations.props -Raw) -replace '<WholeProgramOptimization>true</WholeProgramOptimization>','<WholeProgramOptimization>false</WholeProgramOptimization>') | Set-Content -Path temp\kvazaar-%KVAZAAR_VER%\build\Release_Optimizations.props"

msbuild temp\kvazaar-%KVAZAAR_VER%\build\kvazaar_VS2015.sln /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 || call :buildfailed Kvazaar && exit /b 1

:: Copy results
mkdir ThirdParty\Kvazaar\Lib
mkdir ThirdParty\Kvazaar\Include

robocopy temp\kvazaar-%KVAZAAR_VER% ThirdParty\Kvazaar LICENSE
robocopy temp\kvazaar-%KVAZAAR_VER%\build\x64-Release-libs ThirdParty\Kvazaar\Lib /e
robocopy temp\kvazaar-%KVAZAAR_VER%\src ThirdParty\Kvazaar\Include kvazaar.h

:: Finish
echo Cleaning up Kvazaar files...
rmdir temp\kvazaar-%KVAZAAR_VER% /s /q

echo %COLOR_SUCCESS%Kvazaar successfully set up.%COLOR_RESET%

exit /b 0

:openhevcsetup
call :dependencymissing ThirdParty\OpenHEVC "OpenHEVC" && exit /b 0

:: Set up dependencies
call :visualstudiosetup || exit /b 1
call :yasmsetup || exit /b 1

:: Download
echo Downloading OpenHEVC...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://github.com/OpenHEVC/openHEVC/archive/refs/heads/%OPENHEVC_VER%.zip', 'temp\OpenHEVC.zip')" || call :downloadfailed OpenHEVC && exit /b 1
echo Extracting OpenHEVC...
%powershell% -command "Expand-Archive -Path temp\OpenHEVC.zip -DestinationPath temp -Force"
del temp\OpenHEVC.zip /q

:: Building OpenHEVC is broken on Windows, needs to be patched
echo Patching OpenHEVC...
:: Set updated CMake version and enable C11
%powershell% -command "((Get-Content -path temp\openHEVC-%OPENHEVC_VER%\CMakeLists.txt -Raw) -replace 'cmake_minimum_required \(VERSION 2\.8\)',\""cmake_minimum_required (VERSION 3.10)`nset (CMAKE_C_STANDARD 11)\"") | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\CMakeLists.txt"
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

:: Build
echo Building OpenHEVC...

cmake temp\openHEVC-%OPENHEVC_VER% -Btemp\openHEVC-%OPENHEVC_VER%\build -DYASM_EXECUTABLE="%ROOT_DIR%temp\Yasm\vsyasm.exe" -DYASM_FOUND=True -DENABLE_STATIC=True || call :buildfailed OpenHEVC && exit /b 1
msbuild temp\openHEVC-%OPENHEVC_VER%\build\openHEVC.sln /target:LibOpenHevcWrapper /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 || call :buildfailed OpenHEVC && exit /b 1

:: Copy results
mkdir ThirdParty\OpenHEVC\Lib
mkdir ThirdParty\OpenHEVC\Include

robocopy temp\openHEVC-%OPENHEVC_VER% ThirdParty\OpenHEVC COPYING.LGPLv2.1
robocopy temp\openHEVC-%OPENHEVC_VER%\build\Release ThirdParty\OpenHEVC\Lib /e
robocopy temp\openHEVC-%OPENHEVC_VER%\gpac\modules\openhevc_dec ThirdParty\OpenHEVC\Include openHevcWrapper.h

:: Finish
echo Cleaning up OpenHEVC files...
rmdir temp\openHEVC-%OPENHEVC_VER% /s /q

echo %COLOR_SUCCESS%OpenHEVC successfully set up.%COLOR_RESET%

exit /b 0

:uvgrtpsetup
call :dependencymissing ThirdParty\uvgRTP "uvgRTP" && exit /b 0

:: Set up dependencies
call :visualstudiosetup || exit /b 1

:: Download
echo Downloading uvgRTP...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://github.com/ultravideo/uvgRTP/archive/v%UVGRTP_VER%.zip', 'temp\uvgRTP.zip')" || call :downloadfailed uvgRTP && exit /b 1
echo Extracting uvgRTP...
%powershell% -command "Expand-Archive -Path temp\uvgRTP.zip -DestinationPath temp -Force"
del temp\uvgRTP.zip /q

:: Build
echo Building uvgRTP...

mkdir temp\uvgRTP-%UVGRTP_VER%\build
cmake temp\uvgRTP-%UVGRTP_VER% -Btemp\uvgRTP-%UVGRTP_VER%\build || call :buildfailed uvgRTP && exit /b 1

msbuild temp\uvgRTP-%UVGRTP_VER%\build\uvgrtp.sln /p:Configuration="Release" /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 || call :buildfailed uvgRTP && exit /b 1

:: Copy results
mkdir ThirdParty\uvgRTP\Lib
mkdir ThirdParty\uvgRTP\Include

robocopy temp\uvgRTP-%UVGRTP_VER% ThirdParty\uvgRTP COPYING
robocopy temp\uvgRTP-%UVGRTP_VER%\build\Release ThirdParty\uvgRTP\Lib /e
robocopy temp\uvgRTP-%UVGRTP_VER%\include\uvgrtp ThirdParty\uvgRTP\Include

:: Finish
echo Cleaning up uvgRTP files...
rmdir temp\uvgRTP-%UVGRTP_VER% /s /q

echo %COLOR_SUCCESS%uvgRTP successfully set up.%COLOR_RESET%

exit /b 0

:fpngsetup
call :dependencymissing ThirdParty\fpng "fpng" && exit /b 0

:: Set up dependencies
call :visualstudiosetup || exit /b 1

:: Download
echo Downloading fpng...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://github.com/richgel999/fpng/archive/refs/tags/v%FPNG_VER%.zip', 'temp\fpng.zip')" || call :downloadfailed fpng && exit /b 1
echo Extracting fpng...
%powershell% -command "Expand-Archive -Path temp\fpng.zip -DestinationPath temp -Force"
del temp\fpng.zip /q

:: Build
echo Building fpng...

cl /c /Fotemp\fpng-%FPNG_VER%\fpng.obj /D_MT /D_DLL temp\fpng-%FPNG_VER%\src\fpng.cpp || call :buildfailed fpng && exit /b 0
lib /OUT:temp\fpng-%FPNG_VER%\fpng.lib temp\fpng-%FPNG_VER%\fpng.obj || call :buildfailed fpng && exit /b 0

:: Copy results
mkdir ThirdParty\fpng\Lib
mkdir ThirdParty\fpng\Include

robocopy temp\fpng-%FPNG_VER% ThirdParty\fpng README.md
robocopy temp\fpng-%FPNG_VER% ThirdParty\fpng\Lib fpng.lib
robocopy temp\fpng-%FPNG_VER%\src ThirdParty\fpng\Include fpng.h

:: Finish
echo Cleaning up fpng files...
rmdir temp\fpng-%FPNG_VER% /s /q

echo %COLOR_SUCCESS%fpng successfully set up.%COLOR_RESET%

exit /b 0

:pahocppsetup
call :dependencymissing ThirdParty\PahoCpp "Eclipse Paho" && exit /b 0

:: Set up dependencies
call :visualstudiosetup || exit /b 1
call :opensslsetup || exit /b 1

:: Download
echo Downloading Eclipse Paho C++ library...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://github.com/eclipse-paho/paho.mqtt.cpp/archive/refs/tags/v%PAHO_CPP_VER%.zip', 'temp\pahocpp.zip')" || call :downloadfailed "Eclipse Paho C++ library" && exit /b 1
echo Extracting Eclipse Paho C++ library...
%powershell% -command "Expand-Archive -Path temp\pahocpp.zip -DestinationPath temp -Force"
del temp\pahocpp.zip /q

echo Downloading Eclipse Paho C library...
%powershell% -command "(New-Object Net.WebClient).DownloadFile('https://github.com/eclipse-paho/paho.mqtt.c/archive/refs/tags/v%PAHO_C_VER%.zip', 'temp\pahoc.zip')" || call :downloadfailed "Eclipse Paho C library" && exit /b 1
echo Extracting Eclipse Paho C library...
%powershell% -command "Expand-Archive -Path temp\pahoc.zip -DestinationPath temp\paho.mqtt.cpp-%PAHO_CPP_VER%\externals -Force"
del temp\pahoc.zip /q

rmdir temp\paho.mqtt.cpp-%PAHO_CPP_VER%\externals\paho-mqtt-c /s /q
ren temp\paho.mqtt.cpp-%PAHO_CPP_VER%\externals\paho.mqtt.c-%PAHO_C_VER% paho-mqtt-c

:: Build
echo Building Eclipse Paho...

mkdir temp\paho.mqtt.cpp-%PAHO_CPP_VER%\build
cmake temp\paho.mqtt.cpp-%PAHO_CPP_VER% -Btemp\paho.mqtt.cpp-%PAHO_CPP_VER%\build -DPAHO_WITH_MQTT_C=ON -DPAHO_BUILD_STATIC=ON -DPAHO_WITH_SSL=ON || call :buildfailed "Eclipse Paho" && exit /b 1

msbuild temp\paho.mqtt.cpp-%PAHO_CPP_VER%\build\PahoMqttCpp.sln /p:Configuration="Release" /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 || call :buildfailed "Eclipse Paho" && exit /b 1

:: Copy results
mkdir ThirdParty\PahoCpp\Lib
mkdir ThirdParty\PahoCpp\Include

robocopy temp\paho.mqtt.cpp-%PAHO_CPP_VER% ThirdParty\PahoCpp LICENSE
robocopy temp\paho.mqtt.cpp-%PAHO_CPP_VER%\build\src\Release ThirdParty\PahoCpp\Lib paho-mqttpp3-static.lib
robocopy temp\paho.mqtt.cpp-%PAHO_CPP_VER%\build\externals\paho-mqtt-c\src\Release ThirdParty\PahoCpp\Lib paho-mqtt3as-static.lib
robocopy temp\OpenSSL\x64\lib ThirdParty\PahoCpp\Lib libcrypto.lib
robocopy temp\OpenSSL\x64\lib ThirdParty\PahoCpp\Lib libssl.lib
robocopy temp\paho.mqtt.cpp-%PAHO_CPP_VER%\include ThirdParty\PahoCpp\Include /e
robocopy temp\paho.mqtt.cpp-%PAHO_CPP_VER%\externals\paho-mqtt-c\src ThirdParty\PahoCpp\Include *.h

:: Finish
echo Cleaning up Eclipse Paho files...
rmdir temp\paho.mqtt.cpp-%PAHO_CPP_VER% /s /q

echo %COLOR_SUCCESS%Eclipse Paho successfully set up.%COLOR_RESET%

exit /b 0

:setupstart
set ROOT_DIR=%~dp0
set ANY_FAILED=0

call :powershellsetup || exit /b 1

chdir /d %ROOT_DIR%
if exist temp rmdir temp /s /q
mkdir temp

echo Setting up CiThruS2...

call :contentsetup
:: AirSim is not available for UE 5.6, skip for now
::call :airsimsetup
call :dlsssetup
call :fsrsetup
call :impostorbakersetup
call :kvazaarsetup
call :openhevcsetup
call :uvgrtpsetup
call :fpngsetup
call :pahocppsetup

if not exist Content (
	echo %COLOR_FAILURE%Setup failed, CiThruS2 cannot be used without the content!%COLOR_RESET%
) else if %ANY_FAILED%==1 (
	echo %COLOR_WARNING%Failed to set up some dependencies.%COLOR_RESET% CiThruS2 is most likely still usable, but some features will be disabled. Check the errors above for details. You can try to run this script again, or try to open CiThruS.uproject in Unreal Engine 5 to access the simulation environment.
) else (
	echo %COLOR_SUCCESS%CiThruS2 setup was successful!%COLOR_RESET% Next, open CiThruS.uproject in Unreal Engine 5 to access the simulation environment.
)

if exist temp rmdir temp /s /q
chdir /d %ROOT_DIR%
pause

if %ANY_FAILED%==1 (
	exit /b 1
) else (
	exit /b 0
)
