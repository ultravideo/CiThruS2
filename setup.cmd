@echo off
setlocal

set AIRSIM_VER=2.1.0
set DLSS_VER=DLSS-4.5/2026.02.10_UE5.6_DLSS4.5Plugin_v8.5.0
set FSR_VER=560
set KVAZAAR_VER=2.3.2
set YASM_VER=1.3.0
set UVGRTP_VER=4.0.1
set OPENHEVC_VER=ffmpeg_update
set FPNG_VER=1.0.6
set PAHO_C_VER=1.3.15
set PAHO_CPP_VER=1.5.3
set FOONATHAN_MEMORY_VER=0.7-4
set FASTDDS_VER=3.6.2
set FASTDDSGEN_VER=4.3.0.0
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
chdir /d %ROOT_DIR%

exit /b 0

:buildfailed
echo %COLOR_FAILURE%Failed to build %~1!%COLOR_RESET%
set ANY_FAILED=1
chdir /d %ROOT_DIR%

exit /b 0

:dependencymissing
if "%~1"=="" exit /b 1
if "%~2"=="" exit /b 1

if exist %~1 (
	echo %COLOR_SKIP%%~2 already found at %~1, skipping...%COLOR_RESET% ^(Please delete the folder manually if you'd like to reinstall it.^)
	exit /b 0
)
	
exit /b 1

:downloadfile
if "%~1"=="" exit /b 1
if "%~2"=="" exit /b 1

%POWERSHELL% -command "(New-Object Net.WebClient).DownloadFile(\""%~1\"", \""%~2\"")" && exit /b 0

exit /b 1

:unzip
if "%~1"=="" exit /b 1
if "%~2"=="" exit /b 1

%POWERSHELL% -command "Expand-Archive -Path \""%~1\"" -DestinationPath \""%~2\"" -Force" && exit /b 0

exit /b 1

:findandreplace
:: First argument is file path, second is RegEx pattern to find, third is RegEx pattern to replace with (can be empty)
if "%~1"=="" exit /b 1
if "%~2"=="" exit /b 1

:: Make sure special characters are passed to PowerShell correctly
set "FIND_STR=%~2"
set "FIND_STR=%FIND_STR:""=`""""%"
set "FIND_STR=%FIND_STR:\=`\%"
set "FIND_STR=%FIND_STR:$=`$%"

set "REPLACE_STR=%~3"
:: This replacement syntax fails if the string is empty
if not "%~3" == "" set "REPLACE_STR=%REPLACE_STR:""=`""""%"
if not "%~3" == "" set "REPLACE_STR=%REPLACE_STR:\=`\%"
if not "%~3" == "" set "REPLACE_STR=%REPLACE_STR:$=`$%"

:: This uses PowerShell so that we have access to RegEx
%POWERSHELL% -command "((Get-Content -Path \""%~1\"" -Raw) -replace \""%FIND_STR%\"",\""%REPLACE_STR%\"") | Set-Content -Path \""%~1\""" && exit /b 0

exit /b 1

:findstringinfile
:: First argument is returned variable, second is file path, third is RegEx pattern to find, fourth is capture group index to return (optional, will return whole match if missing)
if "%~2"=="" exit /b 1
if "%~3"=="" exit /b 1

:: Make sure special characters are passed to PowerShell correctly
set "FIND_STR=%~3"
set "FIND_STR=%FIND_STR:""=`""""%"
set "FIND_STR=%FIND_STR:\=`\%"
set "FIND_STR=%FIND_STR:$=`$%"

set RETURN_VALUE=""

:: For some reason the only way to save command results to a variable is to use a for loop or save it to a temporary file...
if "%~4"=="" (
	for /f "delims=" %%i in ('%POWERSHELL% -command "(Select-String -Pattern \""%FIND_STR%\"" -InputObject (Get-Content -Path \""%~2\"" -Raw)).Matches[0].Value"') do set "RETURN_VALUE=%%i"
) else (
	for /f "delims=" %%i in ('%POWERSHELL% -command "(Select-String -Pattern \""%FIND_STR%\"" -InputObject (Get-Content -Path \""%~2\"" -Raw)).Matches[0].Groups[%~4].Value"') do set "RETURN_VALUE=%%i"
)

if "%RETURN_VALUE%"=="" exit /b 1

set "%1=%RETURN_VALUE%"

exit /b 1

:powershellsetup
echo Checking for PowerShell...

set POWERSHELL=powershell
where powershell > nul 2>&1
if errorlevel 1 (
	echo %COLOR_FAILURE%PowerShell is required by this script; please install it.%COLOR_RESET%
	set ANY_FAILED=1
	exit /b 1
)

echo PowerShell found.

exit /b 0

:visualstudiosetup
:: Get VS Command Prompt
if "%VisualStudioVersion%" == "" (
	echo Setting up Visual Studio build tools...
	echo Starting x64 Native Tools Command Prompt for Visual Studio...
	
	:: Try the default installation locations. Microsoft keeps changing the naming schemes for these directories so let's just check all we can find in case they do that again
	for /d %%i in ("%ProgramW6432%\Microsoft Visual Studio\*") do (
		for /d %%j in ("%%i\*") do (
			call "%%j\VC\Auxiliary\Build\vcvars64.bat" 2>nul

			if errorlevel 0 (
				echo Visual Studio build tools successfully set up.
				exit /b 0
			)
		)
	)
	
	echo %COLOR_FAILURE%Could not find x64 Native Tools Command Prompt for Visual Studio automatically. Please open it manually and run this script in it.%COLOR_RESET%
	set ANY_FAILED=1
	
	exit /b 1
)

exit /b 0

:yasmsetup
if exist temp\Yasm (
	exit /b 0
)

echo Setting up Yasm...

:: Yasm is needed to build Kvazaar and OpenHEVC
echo Downloading Yasm...
call :downloadfile "http://www.tortall.net/projects/yasm/releases/vsyasm-%YASM_VER%-win64.zip" "temp\Yasm.zip" || call :downloadfailed yasm && exit /b 1
echo Extracting Yasm...
call :unzip "temp\Yasm.zip" "temp\Yasm"
del temp\Yasm.zip /q

:: Kvazaar finds Yasm through the PATH environment variable
set PATH=%PATH%;"%ROOT_DIR%temp\Yasm\"

echo Yasm successfully set up.

exit /b 0

:findueinstallation
if not "%UE_ROOT_DIR%"=="" exit /b 0

:: Get the UE version CiThruS is using from the project file
call :findstringinfile UE_VER CiThruS.uproject "(?<=""EngineAssociation"":\s"")[0-9]*\.[0-9]*(?="")"

if "%UE_VER%"=="" (
    echo %COLOR_FAILURE%Failed to find Unreal Engine project file!%COLOR_RESET%
	set ANY_FAILED=1
	
	exit /b 1
)

:: LauncherInstalled.dat should list the UE installation directories
call :findstringinfile UE_ROOT_DIR %ProgramData%\Epic\UnrealEngineLauncher\LauncherInstalled.dat "(?<=""InstallLocation"": "")([^^""]*)"",[^^}]*""ArtifactId"":\s*""UE_%UE_VER%""" 1

if "%UE_ROOT_DIR%"=="" (
    echo %COLOR_FAILURE%Failed to find Unreal Engine %UE_VER% installation!%COLOR_RESET%
	set ANY_FAILED=1
	
	exit /b 1
)

set "UE_ROOT_DIR=%UE_ROOT_DIR:\\=\%"

echo %COLOR_SUCCESS%Found Unreal Engine %UE_VER%%COLOR_RESET% at %UE_ROOT_DIR%.

exit /b 0

:opensslsetup
if exist temp\OpenSSL (
	exit /b 0
)

call :findueinstallation || exit /b 1

mkdir temp\OpenSSL\lib
mkdir temp\OpenSSL\include

:: The only way I could get the CiThruS code to link correctly was by
:: forcing it to use the same OpenSSL version that Unreal Engine uses,
:: but Unreal Engine stores them in a different folder structure so
:: CMake can't find them unless we copy them into a new folder like
:: this. If you install another OpenSSL version, it segfaults instantly
:: when any OpenSSL function is called inside CiThruS because of
:: mismatched function signatures
robocopy "%UE_ROOT_DIR%\Engine\Source\ThirdParty\OpenSSL\1.1.1t\include\Win64\VS2015\openssl" temp\OpenSSL\include\openssl /e
robocopy "%UE_ROOT_DIR%\Engine\Source\ThirdParty\OpenSSL\1.1.1t\lib\Win64\VS2015\Release" temp\OpenSSL\lib libssl.lib
robocopy "%UE_ROOT_DIR%\Engine\Source\ThirdParty\OpenSSL\1.1.1t\lib\Win64\VS2015\Release" temp\OpenSSL\lib libcrypto.lib

:: CMake finds OpenSSL through this environment variable
set "OPENSSL_ROOT_DIR=%ROOT_DIR%temp\OpenSSL"

echo OpenSSL successfully set up.

exit /b 0

:contentsetup
call :dependencymissing Content "CiThruS2 content" && exit /b 0

echo Setting up CiThruS2 content...

:: Download
echo Downloading CiThruS2 content...
call :downloadfile "https://ultravideo.fi/sim_env/cithrus2_sim_env_content_%CITHRUS_CONTENT_VER%.zip" "temp\CiThruS2_content.zip" || call :downloadfailed "CiThruS2 content" && exit /b 1
echo Extracting CiThruS2 content...
call :unzip "temp\CiThruS2_content.zip" "."
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

echo Setting up AirSim...

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
call :findandreplace "Plugins\AirSim\Source\SimMode\SimModeBase.cpp" "    this->GetWorld()->SetNewWorldOrigin(FIntVector(player_loc) + this->GetWorld()->OriginLocation);" "    //this->GetWorld()->SetNewWorldOrigin(FIntVector(player_loc) + this->GetWorld()->OriginLocation);"

:: Another patch to remove an unused folder which was accidentally included in the Colosseum release and prevents it from compiling
if exist Plugins\AirSim\Source\AssetCode rmdir Plugins\AirSim\Source\AssetCode /s /q

:: Finish
echo Cleaning up AirSim files...
rmdir temp\Colosseum-%AIRSIM_VER% /s /q

echo %COLOR_SUCCESS%AirSim successfully set up.%COLOR_RESET%

exit /b 0

:dlsssetup
call :dependencymissing Plugins\DLSS "NVIDIA DLSS" && exit /b 0

echo Setting up NVIDIA DLSS...

:: Download
echo Downloading NVIDIA DLSS...
call :downloadfile "https://developer.nvidia.com/downloads/assets/gameworks/downloads/secure/dlss/%DLSS_VER%.zip" "temp\DLSS.zip" || call :downloadfailed "NVIDIA DLSS" && exit /b 1
echo Extracting NVIDIA DLSS...
mkdir temp\DLSS
call :unzip "temp\DLSS.zip" "temp\DLSS"
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

echo Setting up AMD FSR 4...

:: Download
echo Downloading AMD FSR 4...
call :downloadfile "https://gpuopen.com/download-Unreal-Engine-FSR4-plugin/" "temp\FSR.zip" || call :downloadfailed "AMD FSR 4" && exit /b 1
echo Extracting AMD FSR 4...
call :unzip "temp\FSR.zip" "temp"
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

:kvazaarsetup
call :dependencymissing ThirdParty\Kvazaar "Kvazaar" && exit /b 0

echo Setting up Kvazaar...

:: Set up dependencies
echo Setting up dependencies for Kvazaar...

call :visualstudiosetup || exit /b 1
call :yasmsetup || exit /b 1

echo Dependencies for Kvazaar successfully set up.

:: Download
echo Downloading Kvazaar...
call :downloadfile "https://github.com/ultravideo/kvazaar/archive/v%KVAZAAR_VER%.zip" "temp\Kvazaar.zip" || call :downloadfailed Kvazaar && exit /b 1
echo Extracting Kvazaar...
call :unzip "temp\Kvazaar.zip" "temp"
del temp\Kvazaar.zip /q

:: Build
echo Building Kvazaar...

:: Change the build configuration of Kvazaar to disable assembly output: otherwise Kvazaar doesn't work with CiThruS
call :findandreplace "temp\kvazaar-%KVAZAAR_VER%\build\C_Properties.props" "AssemblyAndSourceCode" "NoListing"

:: Change the build configuration of Kvazaar to disable whole program optimization. It keeps causing this issue whenever people use different VS versions https://developercommunity.visualstudio.com/t/cannot-build-after-vs-update-link-error/1348830
call :findandreplace "temp\kvazaar-%KVAZAAR_VER%\build\Release_Optimizations.props" "<WholeProgramOptimization>true</WholeProgramOptimization>" "<WholeProgramOptimization>false</WholeProgramOptimization>"

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

echo Setting up OpenHEVC...

:: Set up dependencies
echo Setting up dependencies for OpenHEVC...

call :visualstudiosetup || exit /b 1
call :yasmsetup || exit /b 1

echo Dependencies for OpenHEVC successfully set up.

:: Download
echo Downloading OpenHEVC...
call :downloadfile "https://github.com/OpenHEVC/openHEVC/archive/refs/heads/%OPENHEVC_VER%.zip" "temp\OpenHEVC.zip" || call :downloadfailed OpenHEVC && exit /b 1
echo Extracting OpenHEVC...
call :unzip "temp\OpenHEVC.zip" "temp"
del temp\OpenHEVC.zip /q

:: Building OpenHEVC is broken on Windows, needs to be patched
echo Patching OpenHEVC...
:: Set updated CMake version and enable C11
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\CMakeLists.txt" "cmake_minimum_required \(VERSION 2\.8\)" "cmake_minimum_required (VERSION 3.10)`nset (CMAKE_C_STANDARD 11)"
:: m library doesn't exist on Windows and isn't needed anyway. Replace with explicitly enabling C11 atomics
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\CMakeLists.txt" "target_link_libraries\(LibOpenHevcWrapper m\)" "if (MSVC)`n`ttarget_compile_options(LibOpenHevcWrapper PRIVATE /experimental:c11atomics)`nendif()"
:: These definitions break MSVC and aren't needed anyway
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\CMakeLists.txt" "if\(WIN32\)\s*add_definitions\([\s\S]*?\)\s*endif\(\)" ""
:: Yasm output file extension is incorrect
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\CMakeLists.txt" "set\(YASM_OBJ ""\${CMAKE_CURRENT_BINARY_DIR}\/\${BASENAME}.o""\)" "set(YASM_OBJ ""${CMAKE_CURRENT_BINARY_DIR}/${BASENAME}.obj"")"
:: Inline assembly is not supported on Windows, clear this file to disable it
%POWERSHELL% -command "'' | Set-Content -Path temp\openHEVC-%OPENHEVC_VER%\libavutil\x86\intreadwrite.h"
:: Many parts of config.h have been written incorrectly for Windows and the configure script that generates it doesn't work either
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in" "#define HAVE_W32THREADS 0\s*#define HAVE_OS2THREADS 0\s*#endif\s*#define HAVE_ATOMICS_GCC 1\s*#define HAVE_ATOMICS_SUNCC 0\s*#define HAVE_ATOMICS_WIN32 0" "#define HAVE_W32THREADS 0`n#define HAVE_OS2THREADS 0`n#define HAVE_ATOMICS_GCC 1`n#define HAVE_ATOMICS_SUNCC 0`n#define HAVE_ATOMICS_WIN32 0`n#endif"
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in" "#define HAVE_FCNTL @FCNTL_H_FOUND@" "#define HAVE_FCNTL 0"
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in" "#define HAVE_LSTAT 1" "#define HAVE_LSTAT 0"
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in" "#define HAVE_SYS_PARAM_H 1" "#define HAVE_SYS_PARAM_H 0"
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in" "#define HAVE_SYSCTL 1" "#define HAVE_SYSCTL 0"
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in" "#define HAVE_MMAP 1" "#define HAVE_MMAP 0"
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in" "#define HAVE_DIRENT_H 1" "#define HAVE_DIRENT_H 0"
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in" "#define HAVE_NANOSLEEP 1" "#define HAVE_NANOSLEEP 0"
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in" "#define HAVE_POSIX_MEMALIGN 1" "#define HAVE_POSIX_MEMALIGN 0"
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\platform\x86\config.h.in" "#define HAVE_MEMALIGN 1" "#define HAVE_MEMALIGN 0"
:: POSIX threads don't exist on Windows but there's a wrapper for them in the files
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\gpac\modules\openhevc_dec\openHevcWrapper.c" "#include ""pthread.h""" "#include ""compat/w32pthreads.h"""
:: ATOMIC_VAR_INIT is deprecated
call :findandreplace "temp\openHEVC-%OPENHEVC_VER%\libavutil\cpu.c" "static atomic_int cpu_flags = ATOMIC_VAR_INIT\(-1\);" "static atomic_int cpu_flags = -1;"

:: Build
echo Building OpenHEVC...

cmake temp\openHEVC-%OPENHEVC_VER% -Btemp\openHEVC-%OPENHEVC_VER%\build -DYASM_EXECUTABLE="%ROOT_DIR%temp\Yasm\vsyasm.exe" -DYASM_FOUND=True -DENABLE_STATIC=True || call :buildfailed OpenHEVC && exit /b 1

if exist temp\openHEVC-%OPENHEVC_VER%\build\openHEVC.sln set OPENHEVC_SLN=temp\openHEVC-%OPENHEVC_VER%\build\openHEVC.sln
if exist temp\openHEVC-%OPENHEVC_VER%\build\openHEVC.slnx set OPENHEVC_SLN=temp\openHEVC-%OPENHEVC_VER%\build\openHEVC.slnx

msbuild %OPENHEVC_SLN% /target:LibOpenHevcWrapper /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 || call :buildfailed OpenHEVC && exit /b 1

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

echo Setting up uvgRTP...

:: Set up dependencies
echo Setting up dependencies for uvgRTP...

call :visualstudiosetup || exit /b 1

echo Dependencies for uvgRTP successfully set up.

:: Download
echo Downloading uvgRTP...
call :downloadfile "https://github.com/ultravideo/uvgRTP/archive/v%UVGRTP_VER%.zip" "temp\uvgRTP.zip" || call :downloadfailed uvgRTP && exit /b 1
echo Extracting uvgRTP...
call :unzip "temp\uvgRTP.zip" "temp"
del temp\uvgRTP.zip /q

:: Build
echo Building uvgRTP...

mkdir temp\uvgRTP-%UVGRTP_VER%\build
cmake temp\uvgRTP-%UVGRTP_VER% -Btemp\uvgRTP-%UVGRTP_VER%\build || call :buildfailed uvgRTP && exit /b 1

if exist temp\uvgRTP-%UVGRTP_VER%\build\uvgrtp.sln set UVGRTP_SLN=temp\uvgRTP-%UVGRTP_VER%\build\uvgrtp.sln
if exist temp\uvgRTP-%UVGRTP_VER%\build\uvgrtp.slnx set UVGRTP_SLN=temp\uvgRTP-%UVGRTP_VER%\build\uvgrtp.slnx

msbuild %UVGRTP_SLN% /p:Configuration="Release" /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 || call :buildfailed uvgRTP && exit /b 1

:: Copy results
mkdir ThirdParty\uvgRTP\Lib
mkdir ThirdParty\uvgRTP\Include

robocopy temp\uvgRTP-%UVGRTP_VER% ThirdParty\uvgRTP COPYING
robocopy temp\uvgRTP-%UVGRTP_VER%\build\Release ThirdParty\uvgRTP\Lib /e
robocopy temp\uvgRTP-%UVGRTP_VER%\include ThirdParty\uvgRTP\Include /e
robocopy temp\uvgRTP-%UVGRTP_VER%\build\include ThirdParty\uvgRTP\Include /e

:: Finish
echo Cleaning up uvgRTP files...
rmdir temp\uvgRTP-%UVGRTP_VER% /s /q

echo %COLOR_SUCCESS%uvgRTP successfully set up.%COLOR_RESET%

exit /b 0

:fpngsetup
call :dependencymissing ThirdParty\fpng "fpng" && exit /b 0

echo Setting up fpng...

:: Set up dependencies
echo Setting up dependencies for fpng...

call :visualstudiosetup || exit /b 1

echo Dependencies for fpng successfully set up.

:: Download
echo Downloading fpng...
call :downloadfile "https://github.com/richgel999/fpng/archive/refs/tags/v%FPNG_VER%.zip" "temp\fpng.zip" || call :downloadfailed fpng && exit /b 1
echo Extracting fpng...
call :unzip "temp\fpng.zip" "temp"
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

echo Setting up Eclipse Paho C++ library...

:: Set up dependencies
echo Setting up dependencies for Eclipse Paho C++ library...

call :visualstudiosetup || exit /b 1
call :opensslsetup || exit /b 1

echo Dependencies for Eclipse Paho C++ library successfully set up.

:: Download
echo Downloading Eclipse Paho C++ library...
call :downloadfile "https://github.com/eclipse-paho/paho.mqtt.cpp/archive/refs/tags/v%PAHO_CPP_VER%.zip" "temp\pahocpp.zip" || call :downloadfailed "Eclipse Paho C++ library" && exit /b 1
echo Extracting Eclipse Paho C++ library...
call :unzip "temp\pahocpp.zip" "temp"
del temp\pahocpp.zip /q

echo Downloading Eclipse Paho C library...
call :downloadfile "https://github.com/eclipse-paho/paho.mqtt.c/archive/refs/tags/v%PAHO_C_VER%.zip" "temp\pahoc.zip" || call :downloadfailed "Eclipse Paho C library" && exit /b 1
echo Extracting Eclipse Paho C library...
call :unzip "temp\pahoc.zip" "temp\paho.mqtt.cpp-%PAHO_CPP_VER%\externals"
del temp\pahoc.zip /q

rmdir temp\paho.mqtt.cpp-%PAHO_CPP_VER%\externals\paho-mqtt-c /s /q
ren temp\paho.mqtt.cpp-%PAHO_CPP_VER%\externals\paho.mqtt.c-%PAHO_C_VER% paho-mqtt-c

:: Build
echo Building Eclipse Paho...

mkdir temp\paho.mqtt.cpp-%PAHO_CPP_VER%\build
cmake temp\paho.mqtt.cpp-%PAHO_CPP_VER% -Btemp\paho.mqtt.cpp-%PAHO_CPP_VER%\build -DPAHO_WITH_MQTT_C=ON -DPAHO_BUILD_STATIC=ON -DPAHO_BUILD_SHARED=OFF -DPAHO_WITH_SSL=ON || call :buildfailed "Eclipse Paho" && exit /b 1

if exist temp\paho.mqtt.cpp-%PAHO_CPP_VER%\build\PahoMqttCpp.sln set PAHO_CPP_SLN=temp\paho.mqtt.cpp-%PAHO_CPP_VER%\build\PahoMqttCpp.sln
if exist temp\paho.mqtt.cpp-%PAHO_CPP_VER%\build\PahoMqttCpp.slnx set PAHO_CPP_SLN=temp\paho.mqtt.cpp-%PAHO_CPP_VER%\build\PahoMqttCpp.slnx

msbuild %PAHO_CPP_SLN% /p:Configuration="Release" /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 || call :buildfailed "Eclipse Paho" && exit /b 1

:: Copy results
mkdir ThirdParty\PahoCpp\Lib
mkdir ThirdParty\PahoCpp\Include

robocopy temp\paho.mqtt.cpp-%PAHO_CPP_VER% ThirdParty\PahoCpp LICENSE
robocopy temp\paho.mqtt.cpp-%PAHO_CPP_VER%\build\src\Release ThirdParty\PahoCpp\Lib paho-mqttpp3-static.lib
robocopy temp\paho.mqtt.cpp-%PAHO_CPP_VER%\build\externals\paho-mqtt-c\src\Release ThirdParty\PahoCpp\Lib paho-mqtt3as-static.lib

robocopy temp\paho.mqtt.cpp-%PAHO_CPP_VER%\include ThirdParty\PahoCpp\Include /s
robocopy temp\paho.mqtt.cpp-%PAHO_CPP_VER%\externals\paho-mqtt-c\src ThirdParty\PahoCpp\Include *.h

:: Finish
echo Cleaning up Eclipse Paho files...
rmdir temp\paho.mqtt.cpp-%PAHO_CPP_VER% /s /q

echo %COLOR_SUCCESS%Eclipse Paho successfully set up.%COLOR_RESET%

exit /b 0

:nvencsetup
call :dependencymissing ThirdParty\NVENC "NVENC" && exit /b 0

echo Setting up NVENC...

:: Set up dependencies
echo Setting up dependencies for NVENC...

call :visualstudiosetup || exit /b 1

echo Dependencies for NVENC successfully set up.

:: Download
mkdir temp\NVENC

echo Downloading NVENC...
:: NVIDIA doesn't officially provide these files without logging in which is impossible to automate, so download them from FFmpeg's public repository instead
call :downloadfile "https://raw.githubusercontent.com/FFmpeg/nv-codec-headers/refs/heads/master/include/ffnvcodec/nvEncodeAPI.h" "temp\NVENC\nvEncodeAPI.h" || call :downloadfailed NVENC && exit /b 1

:: Build
echo Building NVENC...

:: This assumes that nvEncodeAPI64.dll already exists on the computer somewhere, which should be the case if it has NVIDIA drivers
(
	echo LIBRARY nvEncodeAPI64.dll
	echo EXPORTS
	echo.    NvEncodeAPICreateInstance
	echo.    NvEncodeAPIGetMaxSupportedVersion
) > temp\NVENC\nvencodeapi.def

lib /DEF:temp\NVENC\nvencodeapi.def /MACHINE:x64 /OUT:temp\NVENC\nvencodeapi.lib || call :buildfailed NVENC && exit /b 0

:: Copy results
mkdir ThirdParty\NVENC\Lib
mkdir ThirdParty\NVENC\Include

(
	echo https://developer.nvidia.com/nvidia-video-codec-sdk-license-agreement
) > ThirdParty\NVENC\LICENSE

robocopy temp\NVENC ThirdParty\NVENC\Lib nvencodeapi.lib
robocopy temp\NVENC ThirdParty\NVENC\Include nvEncodeAPI.h

:: Finish
echo Cleaning up NVENC files...
rmdir temp\NVENC /s /q

echo %COLOR_SUCCESS%NVENC successfully set up.%COLOR_RESET%

exit /b 0

:fastddssetup
call :dependencymissing ThirdParty\FastDDS "Fast DDS" && exit /b 0

echo Setting up eProsima Fast DDS...

:: Set up dependencies
echo Setting up dependencies for eProsima Fast DDS...

call :visualstudiosetup || exit /b 1
call :findueinstallation || exit /b 1
call :opensslsetup || exit /b 1

echo Downloading Foonathan Memory...
call :downloadfile "https://github.com/foonathan/memory/archive/refs/tags/v%FOONATHAN_MEMORY_VER%.zip" "temp\foonathan_memory.zip" || call :downloadfailed "Foonathan Memory" && exit /b 1
echo Extracting Foonathan Memory...
call :unzip "temp\foonathan_memory.zip" "temp"
del temp\foonathan_memory.zip /q

echo Building Foonathan Memory...
mkdir temp\memory-%FOONATHAN_MEMORY_VER%\build
cmake temp\memory-%FOONATHAN_MEMORY_VER% -Btemp\memory-%FOONATHAN_MEMORY_VER%\build -DCMAKE_INSTALL_PREFIX=%ROOT_DIR%temp\foonathan_memory -DFOONATHAN_MEMORY_BUILD_EXAMPLES=OFF -DFOONATHAN_MEMORY_BUILD_TESTS=OFF || call :buildfailed "Foonathan Memory" && exit /b 1
cmake --build temp\memory-%FOONATHAN_MEMORY_VER%\build --target install --config Release || call :buildfailed "Foonathan Memory" && exit /b 1

:: Fast DDS finds Foonathan Memory through this environment variable
set "foonathan_memory_DIR=%ROOT_DIR%temp\foonathan_memory"

echo Dependencies for eProsima Fast DDS successfully set up.

:: Download
echo Downloading eProsima Fast DDS main repository...
git clone https://github.com/eProsima/Fast-DDS.git temp\Fast-DDS || call :downloadfailed "Fast DDS" && exit /b 1
cd temp\Fast-DDS
git checkout tags/v%FASTDDS_VER%

echo Downloading eProsima Fast DDS submodules...
git submodule init || call :downloadfailed "FastDDS submodules" && exit /b 1
git submodule update || call :downloadfailed "FastDDS submodules" && exit /b 1
cd ..\..

:: Build
echo Building eProsima Fast DDS...
mkdir temp\Fast-DDS\build
cmake temp\Fast-DDS -Btemp\Fast-DDS\build -DCMAKE_INSTALL_PREFIX=%ROOT_DIR%temp\fastdds -DTHIRDPARTY_fastcdr=FORCE -DTHIRDPARTY_Asio=FORCE -DTHIRDPARTY_TinyXML2=FORCE -DSQLITE3_SUPPORT=OFF -DBUILD_SHARED_LIBS=OFF -T v143 || call :buildfailed "Fast DDS" && exit /b 1
cmake --build temp\Fast-DDS\build --target install --config Release || call :buildfailed "Fast DDS" && exit /b 1

:: Fast DDS-Gen doesn't work with newer JDK versions
echo Downloading OpenJDK 19...
call :downloadfile "https://download.java.net/java/GA/jdk19/877d6127e982470ba2a7faa31cc93d04/36/GPL/openjdk-19_windows-x64_bin.zip" "temp\openjdk.zip" || call :downloadfailed "OpenJDK 19" && exit /b 1
echo Extracting OpenJDK 19...
call :unzip "temp\openjdk.zip" "temp"
del temp\openjdk.zip /q

:: gradlew finds Java through this environment variable
set "JAVA_HOME=%ROOT_DIR%temp\jdk-19"

echo Downloading eProsima Fast DDS-Gen...
git clone --recursive https://github.com/eProsima/Fast-DDS-Gen.git temp\fastddsgen || call :downloadfailed "Fast DDS-Gen" && exit /b 1
cd temp\fastddsgen
git checkout tags/v%FASTDDSGEN_VER%

echo Building eProsima Fast DDS-Gen...
call gradlew.bat assemble
:: Gradle keeps running in the background, so stop it
call gradlew.bat --stop
cd ..\..

:: Only check this after the Gradle commands to ensure we don't jump out of this function before the Gradle daemon is stopped and the working directory is back to normal
if not exist temp\fastddsgen\share\fastddsgen\java\fastddsgen.jar (
	call :buildfailed "Fast DDS-Gen" && exit /b 1
)

:: Copy results
mkdir ThirdParty\FastDDS\Bin
mkdir ThirdParty\FastDDS\Lib
mkdir ThirdParty\FastDDS\Include
mkdir ThirdParty\FastDDS\Java

robocopy temp\Fast-DDS ThirdParty\FastDDS LICENSE
robocopy temp\fastdds\lib ThirdParty\FastDDS\Lib *.lib
robocopy temp\foonathan_memory\lib ThirdParty\FastDDS\Lib *.lib
robocopy temp\fastdds\bin ThirdParty\FastDDS\Bin *.dll
robocopy temp\fastdds\include ThirdParty\FastDDS\Include /s

robocopy temp\fastddsgen\share\fastddsgen\java ThirdParty\FastDDS\Bin fastddsgen.jar
robocopy temp\jdk-19 ThirdParty\FastDDS\Java /s

:: Finish
echo Cleaning up eProsima Fast DDS files...
rmdir temp\Fast-DDS /s /q
rmdir temp\memory-%FOONATHAN_MEMORY_VER% /s /q
rmdir temp\foonathan_memory /s /q
rmdir temp\fastdds /s /q
rmdir temp\fastddsgen /s /q
rmdir temp\jdk-19 /s /q

echo %COLOR_SUCCESS%eProsima Fast DDS successfully set up.%COLOR_RESET%

exit /b 0

:setupstart
set ROOT_DIR=%~dp0
set ANY_FAILED=0

:: This checks if this file is being executed directly as setup.cmd or as part of another script
set SCRIPT_CALLED_ALONE=0
echo.%cmdcmdline% | %WINDIR%\system32\find.exe /i "%~0" >nul && set SCRIPT_CALLED_ALONE=1

chdir /d %ROOT_DIR%
if exist temp rmdir temp /s /q
mkdir temp

echo Setting up CiThruS2...

call :powershellsetup || exit /b 1

call :contentsetup
:: AirSim is not available for UE 5.6, skip for now
::call :airsimsetup
call :dlsssetup
call :fsrsetup
call :kvazaarsetup
call :openhevcsetup
call :uvgrtpsetup
call :fpngsetup
call :nvencsetup
call :pahocppsetup
call :fastddssetup

if not exist Content (
	echo %COLOR_FAILURE%Setup failed, CiThruS2 cannot be used without the content!%COLOR_RESET%
) else if %ANY_FAILED%==1 (
	echo %COLOR_WARNING%Failed to set up some dependencies.%COLOR_RESET% CiThruS2 is most likely still usable, but some features will be disabled. Check the errors above for details. You can try to run this script again, or try to open CiThruS.uproject in Unreal Engine 5 to access the simulation environment.
) else (
	echo %COLOR_SUCCESS%CiThruS2 setup was successful!%COLOR_RESET% Next, open CiThruS.uproject in Unreal Engine 5 to access the simulation environment.
)

if exist temp rmdir temp /s /q
chdir /d %ROOT_DIR%

:: Pause so that the window doesn't close automatically and the user can look
:: at the output, but don't pause if this was called as part of another script
if %SCRIPT_CALLED_ALONE%==1 pause

if %ANY_FAILED%==1 (
	exit /b 1
) else (
	exit /b 0
)
