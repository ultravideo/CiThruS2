#!/bin/bash

AIRSIM_VER="2.1.0"
DLSS_VER="DLSS-4.5/2026.02.10_UE5.6_DLSS4.5Plugin_v8.5.0"
FSR_VER="560"
METALIDAR_VER="5.3.2"
KVAZAAR_VER="2.3.2"
YASM_VER="1.3.0"
UVGRTP_VER="4.0.1"
OPENHEVC_VER="ffmpeg_update"
FPNG_VER="1.0.6"
PAHO_C_VER="1.3.15"
PAHO_CPP_VER="1.5.3"
FOONATHAN_MEMORY_VER="0.7-4"
FASTDDS_VER="3.6.2"
FASTDDSGEN_VER="4.3.0.0"
CITHRUS_CONTENT_VER="15_10_2026"

COLOR_FAILURE="\033[1;31m"
COLOR_WARNING="\033[1;33m"
COLOR_SUCCESS="\033[1;32m"
COLOR_SKIP="\033[1;36m"
COLOR_RESET="\033[00m"

download_failed()
{
    echo -e "${COLOR_FAILURE}Failed to download $1!${COLOR_RESET}"
    ANY_FAILED=1

    return 0
}

build_failed()
{
    echo -e "${COLOR_FAILURE}Failed to build $1!${COLOR_RESET}"
    ANY_FAILED=1

    return 0
}

dependency_missing()
{
    if [ -d $1 ]
    then
        echo -e "${COLOR_SKIP}$2 already found at $1, skipping...${COLOR_RESET} (Please delete the folder manually if you'd like to reinstall it.)"
        return 1
    else
        return 0
    fi
}

find_ue_installation()
{
    if [[ ! "${UE_ROOT_DIR}" = "" ]]
    then
        return 0
    fi

    UE_VER=$(grep -Po '(?<="EngineAssociation":\s")[0-9]*\.[0-9]*(?=")' "CiThruS.uproject")

    if [[ "${UE_VER}" = "" ]]
    then
        echo -e "${COLOR_WARNING}Failed to find Unreal Engine project file!${COLOR_RESET}"
        ANY_FAILED=1
        return 1
    fi

    echo "Looking for Unreal Engine ${UE_VER}..."

    # Unreal Engine has no default install location, but most people seem to put it in /opt or /home
    # Might as well search in other places too in case someone has it elsewhere
    UE_BUILD_VERSION_FILE="$(find / \
        -not \( -path /dev -prune \)\
        -not \( -path /proc -prune \)\
        -not \( -path /var -prune \)\
        -not \( -path /sys -prune \)\
        -not \( -path /snap -prune \)\
        -not \( -path /tmp -prune \)\
        -not \( -path /etc -prune \)\
        -not \( -path /root -prune \)\
        -not \( -path /boot -prune \)\
        -not \( -path /usr -prune \)\
        -not \( -path /run -prune \)\
        -not \( -path /lost+found -prune \)\
        -type f -path */Engine/Build/Build.version\
        -exec grep -l \"++UE5+Release-${UE_VER}\" {} +\
        2>/dev/null)"

    if [[ "${UE_BUILD_VERSION_FILE}" = "" ]]
    then
        echo -e "${COLOR_WARNING}Failed to find Unreal Engine ${UE_VER} installation automatically.\
            Unreal Engine is needed to build the dependencies to ensure they are compatible with its custom build environment.${COLOR_RESET}"

        UE_ROOT_DIR=""
        local TRIES=0

        while [[ "${UE_ROOT_DIR}" = "" ]]
        do
            if [[ ${TRIES} = 3 ]]
            then
                echo -e "${COLOR_FAILURE}Please make sure an Unreal Engine ${UE_VER} installation is available on this machine and then try again.${COLOR_RESET}"
                ANY_FAILED=1
                return 1
            fi

            ((TRIES = TRIES + 1))

            echo "Where is Unreal Engine ${UE_VER} installed? Please enter the path to the root of the installation containing the folders Engine, FeaturePacks, Samples etc."
            read UE_ROOT_DIR

            if [[ "${UE_ROOT_DIR}" = "" ]] || [[ ! -f "${UE_ROOT_DIR}/Engine/Build/Build.version" ]]
            then
                echo "That does not seem to be an Unreal Engine installation."
                UE_ROOT_DIR=""
                continue
            fi

            if ! grep -q "++UE5+Release-${UE_VER}" "${UE_ROOT_DIR}/Engine/Build/Build.version"
            then
                echo "That seems to be an installation of a different Unreal Engine version. The required one is ${UE_VER}."
                UE_ROOT_DIR=""
                continue
            fi
        done
    else
        UE_ROOT_DIR=$(realpath $(dirname ${UE_BUILD_VERSION_FILE})/../..)
    fi

    echo -e "${COLOR_SUCCESS}Found Unreal Engine ${UE_VER}${COLOR_RESET} at ${UE_ROOT_DIR}."

    return 0
}

find_clang()
{
    if [[ ! "${CLANG}" = "" ]] && [[ ! "${CLANGPP}" = "" ]] && [[ ! "${CMAKE_UE_BUILD_ENV_ARGS[@]}" = "" ]]
    then
        return 0
    fi

    find_ue_installation || return 1

    # Unreal Engine comes with its own build environment which must be used with the dependencies as well to avoid linker issues
    UE_SYSROOT="${UE_ROOT_DIR}/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v25_clang-18.1.0-rockylinux8/x86_64-unknown-linux-gnu"

    if [[ -d "${UE_SYSROOT}" ]]
    then
        echo -e "${COLOR_SUCCESS}Found Unreal Engine ${UE_VER} build environment${COLOR_RESET} at ${UE_SYSROOT}"
    else
        echo -e "${COLOR_FAILURE}Unreal Engine build environment not found!${COLOR_RESET} Expected to find it at ${UE_SYSROOT}"
        ANY_FAILED=1
        return 1
    fi

    CLANG="${UE_SYSROOT}/bin/clang"

    if [[ -f "${CLANG}" ]]
    then
        echo -e "${COLOR_SUCCESS}Found Clang${COLOR_RESET} at ${CLANG}"
    else
        echo -e "${COLOR_FAILURE}Clang not found!${COLOR_RESET} Expected to find it at ${CLANG}"
        ANY_FAILED=1
        return 1
    fi

    CLANGPP="${UE_SYSROOT}/bin/clang++"
    
    if [[ -f "${CLANGPP}" ]]
    then
        echo -e "${COLOR_SUCCESS}Found Clang++${COLOR_RESET} at ${CLANGPP}"
    else
        echo -e "${COLOR_FAILURE}Clang++ not found!${COLOR_RESET} Expected to find it at ${CLANGPP}"
        ANY_FAILED=1
        return 1
    fi

    declare -ga CMAKE_UE_BUILD_ENV_ARGS
    CMAKE_UE_BUILD_ENV_ARGS+=(-DCMAKE_POSITION_INDEPENDENT_CODE=ON)
    CMAKE_UE_BUILD_ENV_ARGS+=(-DCMAKE_C_COMPILER="${CLANG}")
    CMAKE_UE_BUILD_ENV_ARGS+=(-DCMAKE_CXX_COMPILER="${CLANGPP}")
    CMAKE_UE_BUILD_ENV_ARGS+=(-DCMAKE_C_FLAGS="--sysroot=\"${UE_SYSROOT}\"")
    CMAKE_UE_BUILD_ENV_ARGS+=(-DCMAKE_CXX_FLAGS="--sysroot=\"${UE_SYSROOT}\" -stdlib=libc++")

    return 0
}

find_openssl()
{
    find_ue_installation || return 1

    mkdir -p temp/OpenSSL/lib
    mkdir -p temp/OpenSSL/include

    # The only way I could get the CiThruS code to link correctly was by
    # forcing it to use the same OpenSSL version that Unreal Engine uses,
    # but Unreal Engine stores them in a different folder structure so
    # CMake can't find them unless we copy them into a new folder like
    # this. If you install another OpenSSL version, it segfaults instantly
    # when any OpenSSL function is called inside CiThruS because of
    # mismatched function signatures
    ln -s ${UE_ROOT_DIR}/Engine/Source/ThirdParty/OpenSSL/1.1.1t/include/Unix/openssl temp/OpenSSL/include/openssl
    ln -s ${UE_ROOT_DIR}/Engine/Source/ThirdParty/OpenSSL/1.1.1t/lib/Unix/x86_64-unknown-linux-gnu/libssl.a temp/OpenSSL/lib/libssl.a
    ln -s ${UE_ROOT_DIR}/Engine/Source/ThirdParty/OpenSSL/1.1.1t/lib/Unix/x86_64-unknown-linux-gnu/libcrypto.a temp/OpenSSL/lib/libcrypto.a

    # CMake uses this environment variable to find OpenSSL
    export OPENSSL_ROOT_DIR="${ROOT_DIR}/temp/OpenSSL"
}

setup_content()
{
    if ! dependency_missing Content "CiThruS2 content"
	then
		return 0
	fi

    echo "Setting up CiThruS2 content..."

    # Download
    echo "Downloading CiThruS2 content..."
    wget -O temp/CiThruS2_content.zip https://ultravideo.fi/sim_env/cithrus2_sim_env_content_${CITHRUS_CONTENT_VER}.zip || { download_failed "CiThruS2 content" && return 1; }
    
    unzip temp/CiThruS2_content.zip -d .
    
    rm temp/CiThruS2_content.zip

    # Make editor load regions automatically
    mkdir -p Saved/Config/LinuxEditor
    if [[ ! -f Saved/Config/LinuxEditor/EditorPerProjectUserSettings.ini ]]
    then
        echo "[/Script/Engine.WorldPartitionEditorPerProjectUserSettings]" >> Saved/Config/LinuxEditor/EditorPerProjectUserSettings.ini
        echo "PerWorldEditorSettings=((\"/Game/HervantaMapTemplate/Maps/HervantaSimulation.HervantaSimulation\", (LoadedEditorLocationVolumes=(\"LocationVolume_UAID_18C04D02B040D07301_1649670207\"))))" >> Saved/Config/LinuxEditor/EditorPerProjectUserSettings.ini
    fi

    # Finish
    echo -e "${COLOR_SUCCESS}CiThruS2 content successfully set up.${COLOR_RESET}"
}

setup_airsim()
{
    if ! dependency_missing Plugins/AirSim AirSim
    then
        return 0
    fi

    echo "Setting up AirSim..."

    # Download
    echo "Downloading AirSim (from Colosseum)..."
    git clone https://github.com/ArttuLeppaaho/Colosseum temp/Colosseum || { download_failed "AirSim" && return 1; }
    
    cd temp/Colosseum
    git submodule update --init || { download_failed "AirSim" && return 1; }
    cd ../..

    # Build
    echo Running AirSim setup...
    bash temp/Colosseum/setup.sh || { build_failed "AirSim" && return 1; }

    echo Building AirSim...
    bash temp/Colosseum/build.sh || { build_failed "AirSim" && return 1; }

    # Copy the plugin files
    echo "Copying AirSim plugin files..."
    mkdir -p Plugins

    cp -r temp/Colosseum/Unreal/Plugins Plugins

    # Tiny patch to prevent AirSim from changing the Unreal Engine world origin, which would break CiThruS traffic systems
    sed -i 's|    this->GetWorld()->SetNewWorldOrigin(FIntVector(player_loc) + this->GetWorld()->OriginLocation);|    //this->GetWorld()->SetNewWorldOrigin(FIntVector(player_loc) + this->GetWorld()->OriginLocation);|g' Plugins/AirSim/Source/SimMode/SimModeBase.cpp

    # Another patch to remove an unused folder which was accidentally included in the Colosseum release and prevents it from compiling
    rm -rf Plugins/AirSim/Source/AssetCode

    # Finish
    echo "Cleaning up AirSim files..."
    rm -rf temp/Colosseum-${AIRSIM_VER}

    echo -e "${COLOR_SUCCESS}AirSim successfully set up.${COLOR_RESET}"
}

setup_dlss()
{
    if ! dependency_missing Plugins/DLSS "NVIDIA DLSS"
    then
        return 0
    fi

    echo "Setting up NVIDIA DLSS..."

    # Download
    echo "Downloading NVIDIA DLSS..."
    wget -O temp/DLSS.zip https://developer.nvidia.com/downloads/assets/gameworks/downloads/secure/dlss/${DLSS_VER}.zip || { download_failed "NVIDIA DLSS" && return 1; }

    echo "Extracting NVIDIA DLSS..."
    mkdir temp/DLSS
    unzip temp/DLSS.zip -d temp/DLSS
    rm temp/DLSS.zip

    # Copy the plugin files
    mkdir -p Plugins/

    cp -r temp/DLSS/Plugins/DLSS Plugins/
    cp -r temp/DLSS/Plugins/StreamlineNGXCommon Plugins/

    # Finish
    echo "Cleaning up DLSS files..."
    rm -rf temp/DLSS

    echo -e "${COLOR_SUCCESS}NVIDIA DLSS successfully set up.${COLOR_RESET}"
}

setup_fsr()
{
    if ! dependency_missing Plugins/FSR4 "AMD FSR 4"
    then
        return 0
    fi

    echo "Setting up AMD FSR 4..."

    # Download
    echo "Downloading AMD FSR 4..."
    wget -O temp/FSR.zip https://gpuopen.com/download-Unreal-Engine-FSR4-plugin/ || { download_failed "AMD FSR 4" && return 1; }

    echo "Extracting AMD FSR..."
    mkdir temp/FSR
    unzip temp/FSR.zip -d temp/FSR
    rm temp/FSR.zip

    # Copy the plugin files
    mkdir -p Plugins/
    
    FSR_FOLDER=$(find temp/FSR -maxdepth 1 -type d -regex "temp/FSR/UE-FSR-.*")
    
    cp -r ${FSR_FOLDER}/FSR4-${FSR_VER}/FSR4 Plugins/
    cp -r ${FSR_FOLDER}/FSR4-${FSR_VER}/FSR4MovieRenderPipeline Plugins/
    
    # Finish
    # Make sure we don't delete anything unexpected if the folder is wrong for whatever reason
    if [[ $FSR_FOLDER == temp/FSR/UE-FSR-* ]]
    then
        rm -rf ${FSR_FOLDER}
    fi
    
    echo -e "${COLOR_SUCCESS}AMD FSR 4 successfully set up.${COLOR_RESET}"
}

setup_metalidar()
{
    if ! dependency_missing Plugins/MetaLidar "MetaLidar"
    then
        return 0
    fi

    # Download
    echo "Downloading MetaLidar..."
    wget -O temp/MetaLidar.zip https://github.com/metabotics-ai/MetaLidar/archive/refs/tags/ue${METALIDAR_VER}.zip || { download_failed "MetaLidar" && return 1; }
    echo "Extracting MetaLidar..."
    unzip temp/MetaLidar.zip -d temp
    rm temp/MetaLidar.zip

    mkdir -p Plugins/MetaLidar
    cp -r "temp/MetaLidar-ue${METALIDAR_VER}/." Plugins/MetaLidar

    # patch to make UdpScanComponent accessible to blueprints
    sed -Ei "s/([ \t]*)(class UUDPComponent\* UdpScanComponent;)/\1UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ""MetaLidar"")\n\1\2/" "Plugins/MetaLidar/Source/MetaLidar/Public/LidarBaseActor.h"

    # Finish
    echo "Cleaning up MetaLidar files..."
    rm -rf "temp/MetaLidar-ue${METALIDAR_VER}"

    echo -e "${COLOR_SUCCESS}MetaLidar successfully set up.${COLOR_RESET}"

    return 0
}

setup_udpwrapper()
{
    if ! dependency_missing Plugins/UDPWrapper "UDPWrapper"
    then
        return 0
    fi

    # Download (using git clone because the releases are .7z files...)
    echo "Downloading UDPWrapper..."
    git clone https://github.com/getnamo/UDP-Unreal.git temp/UDPWrapper || { download_failed "UDPWrapper" && return 1; }
    cd temp/UDPWrapper
    git checkout ${UDPWRAPPER_VER} || { download_failed "UDPWrapper" && return 1; }
    cd ../..

    mkdir -p Plugins/
    cp -r temp/UDPWrapper Plugins/

    # Finish
    echo "Cleaning up UDPWrapper files..."
    rm -rf temp/UDPWrapper

    echo -e "${COLOR_SUCCESS}UDPWrapper successfully set up.${COLOR_RESET}"

    return 0
}

setup_kvazaar()
{
    if ! dependency_missing ThirdParty/Kvazaar Kvazaar
    then
        return 0
    fi

    echo "Setting up Kvazaar..."

    # Set up dependencies
    echo "Setting up dependencies for Kvazaar..."

    find_clang || return 1

    echo "Dependencies for Kvazaar successfully set up."

    # Download
    echo "Downloading Kvazaar..."
    wget -O temp/Kvazaar.zip https://github.com/ultravideo/kvazaar/archive/v${KVAZAAR_VER}.zip || { download_failed "Kvazaar" && return 1; }

    echo "Extracting Kvazaar..."
    unzip temp/Kvazaar.zip -d temp
    rm temp/Kvazaar.zip

    # Build
    echo "Building Kvazaar..."

    cmake temp/kvazaar-${KVAZAAR_VER} -Btemp/kvazaar-${KVAZAAR_VER}/build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF "${CMAKE_UE_BUILD_ENV_ARGS[@]}" || { build_failed "Kvazaar" && return 1; }

    cd temp/kvazaar-${KVAZAAR_VER}/build
    make || { build_failed "Kvazaar" && return 1; }
    cd ../../..

    # Copy results
    mkdir -p ThirdParty/Kvazaar/Lib
    mkdir -p ThirdParty/Kvazaar/Include

    cp temp/kvazaar-${KVAZAAR_VER}/LICENSE ThirdParty/Kvazaar
    cp temp/kvazaar-${KVAZAAR_VER}/build/libkvazaar.a ThirdParty/Kvazaar/Lib
    cp temp/kvazaar-${KVAZAAR_VER}/src/kvazaar.h ThirdParty/Kvazaar/Include

    # Finish
    echo "Cleaning up Kvazaar files..."
    rm -rf temp/kvazaar-${KVAZAAR_VER}

    echo -e "${COLOR_SUCCESS}Kvazaar successfully set up.${COLOR_RESET}"
}

setup_openhevc()
{
    if ! dependency_missing ThirdParty/OpenHEVC OpenHEVC
    then
        return 0
    fi

    echo "Setting up OpenHEVC..."

    # Set up dependencies
    echo "Setting up dependencies for OpenHEVC..."

    find_clang || return 1

    echo "Dependencies for OpenHEVC successfully set up."

    # Download
    echo "Downloading OpenHEVC..."
    wget -O temp/OpenHEVC.zip https://github.com/OpenHEVC/openHEVC/archive/refs/heads/${OPENHEVC_VER}.zip || { download_failed "OpenHEVC" && return 1; }

    echo "Extracting OpenHEVC..."
    unzip temp/OpenHEVC.zip -d temp
    rm temp/OpenHEVC.zip

    # Build
    echo "Configuring OpenHEVC..."
    sh temp/configure --disable-asm --enable-pic

    # sysctl.h is deprecated
    sed -i 's|#include <sys/sysctl.h>|//#include <sys/sysctl.h>|g' temp/openHEVC-${OPENHEVC_VER}/libavutil/cpu.c
    # This assembly no longer works
    sed -i -E 's/"(ci|ic)"\s*\(\(uint8_t\)\(?([a-z-]*)\)?\)/"c" \(\2 \& 0x1F\)/g' temp/openHEVC-${OPENHEVC_VER}/libavcodec/x86/mathops.h

    echo "Building OpenHEVC..."

    cmake temp/openHEVC-${OPENHEVC_VER} -Btemp/openHEVC-${OPENHEVC_VER}/build -DENABLE_STATIC=True -DCMAKE_BUILD_TYPE=Release "${CMAKE_UE_BUILD_ENV_ARGS[@]}" || { build_failed "OpenHEVC" && return 1; }

    cd temp/openHEVC-${OPENHEVC_VER}/build
    make LibOpenHevcWrapper || { build_failed "OpenHEVC" && return 1; }
    cd ../../..

    # Copy results
    mkdir -p ThirdParty/OpenHEVC/Lib
    mkdir -p ThirdParty/OpenHEVC/Include

    cp temp/openHEVC-${OPENHEVC_VER}/COPYING.LGPLv2.1 ThirdParty/OpenHEVC
    cp temp/openHEVC-${OPENHEVC_VER}/build/libLibOpenHevcWrapper.a ThirdParty/OpenHEVC/Lib
    cp temp/openHEVC-${OPENHEVC_VER}/gpac/modules/openhevc_dec/openHevcWrapper.h ThirdParty/OpenHEVC/Include

    # Finish
    echo "Cleaning up OpenHEVC files..."
    rm -rf temp/openHEVC-${OPENHEVC_VER}

    echo -e "${COLOR_SUCCESS}OpenHEVC successfully set up.${COLOR_RESET}"
}

setup_uvgrtp()
{
    if ! dependency_missing ThirdParty/uvgRTP uvgRTP
    then
        return 0
    fi

    echo "Setting up uvgRTP..."

    # Set up dependencies
    echo "Setting up dependencies for uvgRTP..."

    find_clang || return 1

    echo "Dependencies for uvgRTP successfully set up."

    # Download
    echo "Downloading uvgRTP..."
    wget -O temp/uvgRTP.tar.gz https://github.com/ultravideo/uvgRTP/archive/v${UVGRTP_VER}.tar.gz || { download_failed "uvgRTP" && return 1; }

    echo "Extracting uvgRTP..."
    tar -xzvf temp/uvgRTP.tar.gz -C temp
    rm temp/uvgRTP.tar.gz

    # Build
    echo "Building uvgRTP..."

    mkdir temp/uvgRTP-${UVGRTP_VER}/Release
    cmake temp/uvgRTP-${UVGRTP_VER} -Btemp/uvgRTP-${UVGRTP_VER}/Release -DCMAKE_BUILD_TYPE=Release "${CMAKE_UE_BUILD_ENV_ARGS[@]}" || { build_failed "uvgRTP" && return 1; }

    cd temp/uvgRTP-${UVGRTP_VER}/Release
    make uvgrtp || { build_failed "uvgRTP" && return 1; }
    cd ../../..

    # Copy results
    mkdir -p ThirdParty/uvgRTP/Lib
    mkdir -p ThirdParty/uvgRTP/Include

    cp temp/uvgRTP-${UVGRTP_VER}/COPYING ThirdParty/uvgRTP
    cp temp/uvgRTP-${UVGRTP_VER}/Release/libuvgrtp.a ThirdParty/uvgRTP/Lib
    cp -r temp/uvgRTP-${UVGRTP_VER}/include/* ThirdParty/uvgRTP/Include
    cp -r temp/uvgRTP-${UVGRTP_VER}/build/include/* ThirdParty/uvgRTP/Include

    # Finish
    echo "Cleaning up uvgRTP files..."
    rm -rf temp/uvgRTP-${UVGRTP_VER}

    echo -e "${COLOR_SUCCESS}uvgRTP successfully set up.${COLOR_RESET}"
}

setup_fpng()
{
    if ! dependency_missing ThirdParty/fpng fpng
    then
        return 0
    fi

    echo "Setting up fpng..."

    # Set up dependencies
    echo "Setting up dependencies for fpng..."

    find_clang || return 1

    echo "Dependencies for fpng successfully set up."

    # Download
    echo "Downloading fpng..."
    wget -O temp/fpng.zip https://github.com/richgel999/fpng/archive/refs/tags/v${FPNG_VER}.zip || { download_failed "fpng" && return 1; }

    echo "Extracting fpng..."
    unzip temp/fpng.zip -d temp
    rm temp/fpng.zip

    # Build
    echo "Building fpng..."

    ${CLANGPP} -c temp/fpng-${FPNG_VER}/src/fpng.cpp -o temp/fpng-${FPNG_VER}/fpng.o -msse4 -mpclmul -fPIC -stdlib=libc++ || { build_failed "fpng" && return 1; }

    ar rvs temp/fpng-${FPNG_VER}/fpng.a temp/fpng-${FPNG_VER}/fpng.o || { build_failed "fpng" && return 1; }

    # Copy results
    mkdir -p ThirdParty/fpng/Lib
    mkdir -p ThirdParty/fpng/Include

    cp temp/fpng-${FPNG_VER}/README.md ThirdParty/fpng
    cp temp/fpng-${FPNG_VER}/src/fpng.h ThirdParty/fpng/Include
    cp temp/fpng-${FPNG_VER}/fpng.a ThirdParty/fpng/Lib

    # Finish
    echo "Cleaning up fpng files..."
    rm -rf temp/fpng-${FPNG_VER}

    echo -e "${COLOR_SUCCESS}fpng successfully set up.${COLOR_RESET}"
}

setup_pahocpp()
{
    if ! dependency_missing ThirdParty/PahoCpp "Eclipse Paho"
    then
        return 0
    fi

    echo "Setting up Eclipse Paho..."

    # Set up dependencies
    echo "Setting up dependencies for Eclipse Paho..."

    find_clang || return 1
    find_openssl || return 1

    echo "Dependencies for Eclipse Paho successfully set up."

    # Download
    echo "Downloading Eclipse Paho C++ library..."
    wget -O temp/pahocpp.tar.gz https://github.com/eclipse-paho/paho.mqtt.cpp/archive/refs/tags/v${PAHO_CPP_VER}.tar.gz || { download_failed "Eclipse Paho C++ library" && return 1; }

    echo "Extracting Eclipse Paho C++ library..."
    tar -xzvf temp/pahocpp.tar.gz -C temp
    rm temp/pahocpp.tar.gz

    # The C++ library is built on top of the C library, so it is needed as well
    echo "Downloading Eclipse Paho C library..."
    wget -O temp/pahoc.tar.gz https://github.com/eclipse-paho/paho.mqtt.c/archive/refs/tags/v${PAHO_C_VER}.tar.gz || { download_failed "Eclipse Paho C library" && return 1; }

    echo "Extracting Eclipse Paho C library..."
    tar -xzvf temp/pahoc.tar.gz -C temp
    rm temp/pahoc.tar.gz

    # Paho C++ finds Paho C from this subdirectory
    rm -rf temp/paho.mqtt.cpp-${PAHO_CPP_VER}/externals/paho-mqtt-c
    mv temp/paho.mqtt.c-${PAHO_C_VER} temp/paho.mqtt.cpp-${PAHO_CPP_VER}/externals/paho-mqtt-c

    # Build
    echo "Building Eclipse Paho..."

    mkdir temp/paho.mqtt.cpp-${PAHO_CPP_VER}/build
    cmake temp/paho.mqtt.cpp-${PAHO_CPP_VER} -Btemp/paho.mqtt.cpp-${PAHO_CPP_VER}/build -DPAHO_WITH_MQTT_C=ON -DPAHO_BUILD_STATIC=ON -DPAHO_BUILD_SHARED=OFF -DPAHO_WITH_SSL=ON "${CMAKE_UE_BUILD_ENV_ARGS[@]}" || { build_failed "Eclipse Paho" && return 1; }
    
    cd temp/paho.mqtt.cpp-${PAHO_CPP_VER}/build
    make paho-mqttpp3-static || { build_failed "Eclipse Paho" && return 1; }
    cd ../../..

    # Copy results
    mkdir -p ThirdParty/PahoCpp/Lib
    mkdir -p ThirdParty/PahoCpp/Include

    cp temp/paho.mqtt.cpp-${PAHO_CPP_VER}/LICENSE ThirdParty/PahoCpp
    cp temp/paho.mqtt.cpp-${PAHO_CPP_VER}/build/src/libpaho-mqttpp3.a ThirdParty/PahoCpp/Lib
    cp temp/paho.mqtt.cpp-${PAHO_CPP_VER}/build/externals/paho-mqtt-c/src/libpaho-mqtt3as.a ThirdParty/PahoCpp/Lib
    cp -r temp/paho.mqtt.cpp-${PAHO_CPP_VER}/include/* ThirdParty/PahoCpp/Include
    cp temp/paho.mqtt.cpp-${PAHO_CPP_VER}/externals/paho-mqtt-c/src/*.h ThirdParty/PahoCpp/Include

    # Finish
    echo "Cleaning up Eclipse Paho files..."
    rm -rf temp/paho.mqtt.cpp-${PAHO_CPP_VER}

    echo -e "${COLOR_SUCCESS}Eclipse Paho successfully set up.${COLOR_RESET}"
}

setup_fastdds()
{
    if ! dependency_missing ThirdParty/FastDDS "eProsima Fast DDS"
    then
        return 0
    fi

    echo "Setting up eProsima Fast DDS..."

    # Set up dependencies
    echo "Setting up dependencies for eProsima Fast DDS..."

    find_clang || return 1
    find_openssl || return 1

    echo "Downloading Foonathan Memory..."
    wget -O temp/foonathan_memory.tar.gz https://github.com/foonathan/memory/archive/refs/tags/v${FOONATHAN_MEMORY_VER}.tar.gz || { download_failed "Foonathan Memory" && return 1; }
    echo "Extracting Foonathan Memory..."
    tar -xzvf temp/foonathan_memory.tar.gz -C temp
    rm temp/foonathan_memory.tar.gz

    echo "Building Foonathan Memory..."
    mkdir temp/memory-${FOONATHAN_MEMORY_VER}/build
    cmake temp/memory-${FOONATHAN_MEMORY_VER} -Btemp/memory-${FOONATHAN_MEMORY_VER}/build -DCMAKE_INSTALL_PREFIX="${ROOT_DIR}/temp/foonathan_memory" -DFOONATHAN_MEMORY_BUILD_EXAMPLES=OFF -DFOONATHAN_MEMORY_BUILD_TESTS=OFF "${CMAKE_UE_BUILD_ENV_ARGS[@]}" || { build_failed "Foonathan Memory" && return 1; }
    cmake --build temp/memory-${FOONATHAN_MEMORY_VER}/build --target install --config Release || { build_failed "Foonathan Memory" && return 1; }

    # Fast DDS finds Foonathan Memory through this environment variable
    export foonathan_memory_DIR="${ROOT_DIR}/temp/foonathan_memory"

    echo "Dependencies for eProsima Fast DDS successfully set up."

    # Download
    echo "Downloading eProsima Fast DDS main repository..."
    git clone https://github.com/eProsima/Fast-DDS.git temp/Fast-DDS || { download_failed "Fast DDS" && return 1; }
    cd temp/Fast-DDS
    git checkout tags/v${FASTDDS_VER}

    echo "Downloading eProsima Fast DDS submodules..."
    git submodule init || { download_failed "FastDDS submodules" && return 1; }
    git submodule update || { download_failed "FastDDS submodules" && return 1; }
    cd ../..

    # Build
    echo "Building eProsima Fast DDS..."
    mkdir temp/Fast-DDS/build
    cmake temp/Fast-DDS -Btemp/Fast-DDS/build -DCMAKE_INSTALL_PREFIX="${ROOT_DIR}/temp/fastdds" -DTHIRDPARTY_fastcdr=FORCE -DTHIRDPARTY_Asio=FORCE -DTHIRDPARTY_TinyXML2=FORCE -DSQLITE3_SUPPORT=OFF -DBUILD_SHARED_LIBS=OFF "${CMAKE_UE_BUILD_ENV_ARGS[@]}" || { build_failed "Fast DDS" && return 1; }
    cmake --build temp/Fast-DDS/build --target install --config Release || { build_failed "Fast DDS" && return 1; }
    
    # Fast DDS-Gen doesn't work with newer JDK versions
    echo "Downloading OpenJDK 19..."
    wget -O temp/openjdk.tar.gz https://download.java.net/java/GA/jdk19/877d6127e982470ba2a7faa31cc93d04/36/GPL/openjdk-19_linux-x64_bin.tar.gz || { download_failed "OpenJDK 19" && return 1; }
    echo "Extracting OpenJDK 19..."
    tar -xzvf temp/openjdk.tar.gz -C temp
    rm temp/openjdk.tar.gz

    # gradlew finds Java through this environment variable
    export JAVA_HOME="${ROOT_DIR}/temp/jdk-19"

    echo "Downloading eProsima Fast DDS-Gen..."
    git clone --recursive https://github.com/eProsima/Fast-DDS-Gen.git temp/fastddsgen || { download_failed "Fast DDS-Gen" && return 1; }
    cd temp/fastddsgen
    git checkout tags/v${FASTDDSGEN_VER}

    echo "Building eProsima Fast DDS-Gen..."
    ./gradlew assemble
    # Gradle keeps running in the background, so stop it
    ./gradlew --stop
    cd ../..

    # Only check this after the Gradle commands to ensure we don't jump out of this function before the Gradle daemon is stopped and the working directory is back to normal
    if [[ ! -f temp/fastddsgen/share/fastddsgen/java/fastddsgen.jar ]]
    then
        build_failed "Fast DDS-Gen" && return 1
    fi

    # Copy results
    mkdir -p ThirdParty/FastDDS/Bin
    mkdir -p ThirdParty/FastDDS/Lib
    mkdir -p ThirdParty/FastDDS/Include
    mkdir -p ThirdParty/FastDDS/Java

    cp temp/Fast-DDS/LICENSE ThirdParty/FastDDS
    cp temp/fastdds/lib/*.a ThirdParty/FastDDS/Lib
    cp temp/foonathan_memory/lib/*.a ThirdParty/FastDDS/Lib
    cp -r temp/fastdds/include/* ThirdParty/FastDDS/Include

    cp temp/fastddsgen/share/fastddsgen/java/fastddsgen.jar ThirdParty/FastDDS/Bin 
    cp -r temp/jdk-19/* ThirdParty/FastDDS/Java

    # Finish
    echo "Cleaning up eProsima Fast DDS files..."
    rm -rf temp/Fast-DDS
    rm -rf temp/memory-${FOONATHAN_MEMORY_VER}
    rm -rf temp/foonathan_memory
    rm -rf temp/fastdds
    rm -rf temp/fastddsgen
    rm -rf temp/jdk-19

    echo -e "${COLOR_SUCCESS}eProsima Fast DDS successfully set up.${COLOR_RESET}"
}

ROOT_DIR=$(pwd)
ANY_FAILED=0

cd "${ROOT_DIR}"

rm -rf temp
mkdir temp

echo "Setting up CiThruS2..."

setup_content
# setup_airsim (Skip this for now, AirSim is outdated)
setup_dlss
# setup_fsr (The FSR plugin does not actually compile on Linux, so skip it)
setup_metalidar
setup_udpwrapper
setup_kvazaar
setup_openhevc
setup_uvgrtp
setup_fpng
setup_pahocpp
setup_fastdds

if [[ ! -d "Content" ]]
then
	echo -e "${COLOR_FAILURE}Setup failed, CiThruS2 cannot be used without the content!${COLOR_RESET}"
elif [[ ${ANY_FAILED} = 1 ]]
then
	echo -e "${COLOR_WARNING}Failed to set up some dependencies.${COLOR_RESET} CiThruS2 is most likely still usable, but some features will be disabled. Check the errors above for details. You can try to run this script again, or try to open CiThruS.uproject in Unreal Engine 5 to access the simulation environment.${COLOR_RESET}"
else
	echo -e "${COLOR_SUCCESS}CiThruS2 setup was successful!${COLOR_RESET} Next, open CiThruS.uproject in Unreal Engine 5 to access the simulation environment."
fi

rm -rf temp
cd "${ROOT_DIR}"

if [[ ${ANY_FAILED} = 1 ]]
then
    exit 1
else
    exit 0
fi
