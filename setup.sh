#!/bin/bash

AIRSIM_VER="2.1.0"
DLSS_VER="2025.09.16_UE5.5_DLSS4Plugin_v8.3.0"
FSR_VER="520"
KVAZAAR_VER="2.3.2"
YASM_VER="1.3.0"
UVGRTP_VER="3.1.6"
OPENHEVC_VER="ffmpeg_update"
FPNG_VER="1.0.6"
CITHRUS_CONTENT_VER="23_09_2025"

# clang must be used to compile the dependencies because Unreal Engine uses
# clang and it will not be able to link the standard libraries correctly if we
# mix gcc and clang
CLANG_VER="18"

dependency_missing()
{
    if [ -d $1 ]
    then
        echo "$2 already found at $1, skipping... (Please delete the folder manually if you'd like to reinstall it.)"
        return 1
    else
        return 0
    fi
}

setup_dependencies()
{
    rm -rf temp
    mkdir temp

    echo "Setting up CiThruS2..."
	
	# CiThruS2 content setup
	if dependency_missing Content "CiThruS2 content"
	then
		echo "Downloading CiThruS2 content..."
        if ! wget -O temp/CiThruS2_content.zip https://ultravideo.fi/sim_env/cithrus2_sim_env_content_${CITHRUS_CONTENT_VER}.zip
        then
            echo "Failed to download CiThruS2 content!"
            return 1
        fi
		
		unzip temp/CiThruS2_content.zip -d .
		
		rm temp/CiThruS2_content.zip

		echo "Finished setting up CiThruS2 content."
	fi

    # AirSim setup
    # (Skip this for now, AirSim is outdated)
    if false && dependency_missing Plugins/AirSim AirSim
    then
        echo "Downloading AirSim (from Colosseum)..."
        if ! git clone https://github.com/ArttuLeppaaho/Colosseum temp/Colosseum
        then
            echo "Failed to download AirSim!"
            return 1
        fi
        
        cd temp/Colosseum
        git submodule update --init
        cd ../..

        echo Running AirSim setup...
        if ! bash temp/Colosseum/setup.sh
        then
            echo "Failed to setup AirSim!"
            return 1
        fi

        echo Building AirSim...
        if ! bash temp/Colosseum/build.sh
        then
            echo "Failed to build AirSim!"
            return 1
        fi

        echo "Copying AirSim plugin files..."
        mkdir -p Plugins

        cp -r temp/Colosseum/Unreal/Plugins Plugins

        # Tiny patch to prevent AirSim from changing the Unreal Engine world origin, which would break CiThruS traffic systems
        sed -i 's|    this->GetWorld()->SetNewWorldOrigin(FIntVector(player_loc) + this->GetWorld()->OriginLocation);|    //this->GetWorld()->SetNewWorldOrigin(FIntVector(player_loc) + this->GetWorld()->OriginLocation);|g' Plugins/AirSim/Source/SimMode/SimModeBase.cpp

        # Another patch to remove an unused folder which was accidentally included in the Colosseum release and prevents it from compiling
        rm -rf Plugins/AirSim/Source/AssetCode

        echo "Cleaning up AirSim files..."
        rm -rf temp/Colosseum-${AIRSIM_VER}

        echo "AirSim successfully set up."
    fi

    # DLSS setup
    if dependency_missing Plugins/DLSS "Nvidia DLSS"
    then
        echo "Downloading Nvidia DLSS..."
        if ! wget -O temp/DLSS.zip https://dlss.download.nvidia.com/uebinarypackages/${DLSS_VER}.zip
        then
            echo "Failed to download Nvidia DLSS!"
            return 1
        fi

        echo "Extracting Nvidia DLSS..."
        mkdir temp/DLSS
        unzip temp/DLSS.zip -d temp/DLSS
        rm temp/DLSS.zip

        mkdir -p Plugins/

        cp -r temp/DLSS/Plugins/DLSS Plugins/

        rm -rf temp/DLSS

        echo "Nvidia DLSS successfully set up."
    fi

    # FSR 2 setup
    if false && dependency_missing Plugins/FSR2 "AMD FSR 2"
    then
        # TODO: FSR 2 is outdated, we have to switch to FSR 4

        echo "AMD FSR 2 successfully set up."
    fi

    # ImpostorBaker setup
    if dependency_missing Plugins/ImpostorBaker-master ImpostorBaker
    then
        echo Downloading ImpostorBaker...
        if ! wget -O temp/ImpostorBaker.zip https://github.com/ictusbrucks/ImpostorBaker/archive/refs/heads/master.zip
        then
            echo "Failed to download ImpostorBaker!"
            return 1
        fi

        echo "Extracting ImpostorBaker..."
        mkdir -p Plugins/
        unzip temp/ImpostorBaker.zip -d Plugins
        rm temp/ImpostorBaker.zip

        echo "ImpostorBaker successfully set up."
    fi

    # Kvazaar setup
    if dependency_missing ThirdParty/Kvazaar Kvazaar
    then
        echo "Downloading Kvazaar..."
        if ! wget -O temp/Kvazaar.zip https://github.com/ultravideo/kvazaar/archive/v${KVAZAAR_VER}.zip
        then
            echo "Failed to download Kvazaar!"
            return 1
        fi

        echo "Extracting Kvazaar..."
        unzip temp/Kvazaar.zip -d temp
        rm temp/Kvazaar.zip

        echo "Building Kvazaar..."

        if ! cmake temp/kvazaar-${KVAZAAR_VER} -Btemp/kvazaar-${KVAZAAR_VER}/build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_C_COMPILER=clang-${CLANG_VER}
        then
            echo "Failed to build Kvazaar!"
            return 1
        fi

        cd temp/kvazaar-${KVAZAAR_VER}/build
        if ! make
        then
            echo "Failed to build Kvazaar!"
            return 1
        fi
		cd ../../..

        mkdir -p ThirdParty/Kvazaar/Lib
        mkdir -p ThirdParty/Kvazaar/Include

        cp temp/kvazaar-${KVAZAAR_VER}/LICENSE ThirdParty/Kvazaar
        cp temp/kvazaar-${KVAZAAR_VER}/build/libkvazaar.a ThirdParty/Kvazaar/Lib
        cp temp/kvazaar-${KVAZAAR_VER}/src/kvazaar.h ThirdParty/Kvazaar/Include

        echo "Cleaning up Kvazaar files..."
        rm -rf temp/kvazaar-${KVAZAAR_VER}

        echo "Kvazaar successfully set up."
    fi

    # OpenHEVC setup
    if dependency_missing ThirdParty/OpenHEVC OpenHEVC
    then
        echo "Downloading OpenHEVC..."
        if ! wget -O temp/OpenHEVC.zip https://github.com/OpenHEVC/openHEVC/archive/refs/heads/${OPENHEVC_VER}.zip
        then
            echo "Failed to download OpenHEVC!"
        fi

        echo "Extracting OpenHEVC..."
        unzip temp/OpenHEVC.zip -d temp
        rm temp/OpenHEVC.zip

        sh temp/configure --disable-asm --enable-pic

        # sysctl.h is deprecated
        sed -i 's|#include <sys/sysctl.h>|//#include <sys/sysctl.h>|g' temp/openHEVC-${OPENHEVC_VER}/libavutil/cpu.c
        # This assembly no longer works
        sed -i -E 's/"(ci|ic)"\s*\(\(uint8_t\)\(?([a-z-]*)\)?\)/"c" \(\2 \& 0x1F\)/g' temp/openHEVC-${OPENHEVC_VER}/libavcodec/x86/mathops.h

        echo "Building OpenHEVC..."

        if ! cmake temp/openHEVC-${OPENHEVC_VER} -Btemp/openHEVC-${OPENHEVC_VER}/build -DENABLE_STATIC=True -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_C_COMPILER=clang-${CLANG_VER}
        then
            echo "Failed to build OpenHEVC!"
            return 1
        fi

        cd temp/openHEVC-${OPENHEVC_VER}/build
        if ! make LibOpenHevcWrapper
        then
            echo "Failed to build OpenHEVC!"
            return 1
        fi
		cd ../../..

        mkdir -p ThirdParty/OpenHEVC/Lib
        mkdir -p ThirdParty/OpenHEVC/Include

        cp temp/openHEVC-${OPENHEVC_VER}/COPYING.LGPLv2.1 ThirdParty/OpenHEVC
        cp temp/openHEVC-${OPENHEVC_VER}/build/libLibOpenHevcWrapper.a ThirdParty/OpenHEVC/Lib
        cp temp/openHEVC-${OPENHEVC_VER}/gpac/modules/openhevc_dec/openHevcWrapper.h ThirdParty/OpenHEVC/Include

        echo "Cleaning up OpenHEVC files..."
        rm -rf temp/openHEVC-${OPENHEVC_VER}

        echo "OpenHEVC successfully set up."
    fi

    # uvgRTP setup
    if dependency_missing ThirdParty/uvgRTP uvgRTP
    then
        echo "Downloading uvgRTP..."
        if ! wget -O temp/uvgRTP.tar.gz https://github.com/ultravideo/uvgRTP/archive/v${UVGRTP_VER}.tar.gz
        then
            echo "Failed to download uvgRTP!"
            return 1
        fi

        echo "Extracting uvgRTP..."
        tar -xzvf temp/uvgRTP.tar.gz -C temp
        rm temp/uvgRTP.tar.gz

        echo "Building uvgRTP..."

        mkdir temp/uvgRTP-${UVGRTP_VER}/Release
        if ! cmake temp/uvgRTP-${UVGRTP_VER} -Btemp/uvgRTP-${UVGRTP_VER}/Release -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_CXX_COMPILER=clang++-${CLANG_VER} -DCMAKE_CXX_FLAGS=-stdlib=libc++
        then
            echo "Failed to build uvgRTP!"
            return 1
        fi

		cd temp/uvgRTP-${UVGRTP_VER}/Release
        if ! make uvgrtp
        then
            echo "Failed to build uvgRTP!"
            return 1
        fi
		cd ../../..

        mkdir -p ThirdParty/uvgRTP/Lib
        mkdir -p ThirdParty/uvgRTP/Include

        cp temp/uvgRTP-${UVGRTP_VER}/COPYING ThirdParty/uvgRTP
        cp temp/uvgRTP-${UVGRTP_VER}/Release/libuvgrtp.a ThirdParty/uvgRTP/Lib
        cp temp/uvgRTP-${UVGRTP_VER}/include/uvgrtp/* ThirdParty/uvgRTP/Include

        echo "Cleaning up uvgRTP files..."
        rm -rf temp/uvgRTP-${UVGRTP_VER}

        echo "Finished setting up uvgRTP."
    fi

    # fpng setup
    if dependency_missing ThirdParty/fpng fpng
    then
        echo "Downloading fpng..."
        if ! wget -O temp/fpng.zip https://github.com/richgel999/fpng/archive/refs/tags/v${FPNG_VER}.zip
        then
            echo "Failed to download fpng!"
            return 1
        fi

        echo "Extracting fpng..."
        unzip temp/fpng.zip -d temp
        rm temp/fpng.zip

        echo "Building fpng..."

        if ! clang++-${CLANG_VER} -c temp/fpng-${FPNG_VER}/src/fpng.cpp -o temp/fpng-${FPNG_VER}/fpng.o -msse4 -mpclmul -fPIC -stdlib=libc++
        then
            echo "Failed to build fpng!"
            return 1
        fi

        if ! ar rvs temp/fpng-${FPNG_VER}/fpng.a temp/fpng-${FPNG_VER}/fpng.o
        then
            echo "Failed to create static library of fpng!"
            return 1
        fi

        mkdir -p ThirdParty/fpng/Lib
        mkdir -p ThirdParty/fpng/Include

        cp temp/fpng-${FPNG_VER}/README.md ThirdParty/fpng
        cp temp/fpng-${FPNG_VER}/src/fpng.h ThirdParty/fpng/Include
        cp temp/fpng-${FPNG_VER}/fpng.a ThirdParty/fpng/Lib

        echo "Cleaning up fpng files..."
        rm -rf temp/fpng-${FPNG_VER}

        echo "Finished setting up fpng."
    fi
}

ROOT_DIR=$(pwd)

cd ${ROOT_DIR}

if setup_dependencies
then
    echo "CiThruS2 setup was successful! Next, open CiThruS.uproject in Unreal Engine 5 to access the simulation environment."
fi

rm -rf temp
cd ${ROOT_DIR}

