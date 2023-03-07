# CiThruS2 Simulation Environment

CiThruS2 (See-Through Sight 2) is a virtual city environment and framework built in Unreal Engine 4 to aid in testing and developing various traffic-related systems, drone systems and vision-based systems. The environment is based on the real city of Hervanta, Finland, and it supports features such as simulated cars, drones and pedestrians, weather, seasonal effects, a changeable time of day and real-time video recording and streaming.

## Important Note

Please note that the content of CiThruS2 is hosted separately on our website [here](https://ultravideo.fi/sim_env/cithrus2_sim_env_content_06_03_2023.zip) due to GitHub storage limitations. The code of the CiThruS2 can be built and used on its own, but the actual Hervanta city environment is located in the content archive instead.

## Build Instructions

Windows is required to run the simulation due to DirectX being used by some of our systems.

Fetch the kvazaar and uvgRTP git submodules and build them inside the Submodules folder. Build both in the Release x64 configuration in Visual Studio to ensure the library files are compatible. For Kvazaar, you may have to disable assembler output in Visual Studio from Project Properties -> C/C++ -> Output Files -> Set Assembler Output to "No Listing". 

Clone or download the [AirSim repository](https://github.com/microsoft/AirSim) and run build.cmd in its files. After building, copy the files from Unreal/Plugins/Airsim in the AirSim repository to Plugins/Airsim in the CiThruS2 repository.

CiThruS2 currently uses Unreal Engine 4.26. It needs to be installed through Epic Games' tools to open the project files.

After installing Unreal Engine 4.26, obtaining the AirSim plugin files and building kvazaar and uvgRTP, open the `HervantaUE4.uproject` project file and click "Yes" when it asks if you want to build. If the build succeeds, the editor will automatically open the environment. The editor will take several minutes to open and it will build shaders for 2min ~ 30min depending on the hardware used, but subsequent startups will be faster. If you also downloaded the CiThruS2 content, the Hervanta environment will open automatically.


## Paper

If you are using CiThruS2 in your research, please refer to the following paper: 

`E. Gałązka, T. T. Niemirepo, and J. Vanne, “CiThruS2: Open-source Photorealistic 3D Framework for Driving and Traffic Simulation in Real Time,” Accepted to IEEE Int. Conf. Intell. Transp., Indianapolis, Indiana, USA, Sept. 2021. `