# CiThruS2 Simulation Environment

CiThruS2 (See-Through Sight 2) is a virtual city environment and framework built in Unreal Engine 4 to aid in testing and developing various traffic-related systems, drone systems and vision-based systems. The environment is based on the real city of Hervanta, Finland, and it supports features such as simulated cars, drones and pedestrians, weather, seasonal effects, a changeable time of day and real-time video recording and streaming.

## First-Time Setup Instructions

To use the environment, you must first make sure PowerShell, Visual Studio 2019, CMake and Unreal Engine 4.26 are installed on your computer. Then, run the `setup.cmd` script, which will automatically set up dependencies and download the CiThruS2 content, which is not included in this repository due to GitHub storage limitations. Please note that the script will download ~10GB of data: make sure your computer has enough storage space.

After successfully running the script, open the `HervantaUE4.uproject` project file in Unreal Engine. Click "Yes" when it asks whether you want to build the code. After building, the Unreal Engine editor will automatically open the environment. The editor will take several minutes to open and it will build shaders for 2min ~ 30min depending on the hardware used, but subsequent startups will be faster. If the editor fails to open or the build process fails, please check that the setup script ran successfully and did not generate errors. If you encounter a linker error, it might be because you have multiple Visual Studio 2019 toolchains installed: deleting all but the latest toolchain should allow the project to build.

If you wish to delete CiThruS2, simply delete the directory in which you downloaded this repository: the setup script does not install anything elsewhere on your computer (other than CMake if it was not already installed).

## Paper

If you are using CiThruS2 in your research, please refer to the following paper: 

`E. Gałązka, T. T. Niemirepo, and J. Vanne, “CiThruS2: Open-source Photorealistic 3D Framework for Driving and Traffic Simulation in Real Time,” Accepted to IEEE Int. Conf. Intell. Transp., Indianapolis, Indiana, USA, Sept. 2021. `
