# CiThruS2 Simulation Environment

CiThruS2 (See-Through Sight 2) is a virtual city environment and framework built in Unreal Engine 4 to aid in testing and developing various traffic-related systems, drone systems and vision-based systems. The environment is based on the real city of Hervanta, Finland, and it supports features such as simulated cars, drones and pedestrians, weather, seasonal effects, a changeable time of day and real-time video recording and streaming.

## First-Time Setup Instructions

To use the environment, you must first run the setup script in `setup.cmd`, which will automatically set up dependencies and download the CiThruS2 content, which is not included in this repository due to GitHub storage limitations. Currently, the environment is only available for Windows.

Before running the script, make sure PowerShell and cmake are installed on your computer. Then, run `setup.cmd` using x64 Native Tools Command Prompt for VS 2019. VS 2022 should work too, but hasn't been tested. Please note that the script will download ~8GB of data: make sure your computer has enough storage space.

After successfully running the script, install Unreal Engine 4.26 and open the `HervantaUE4.uproject` project file with it. Click "Yes" when it asks whether you want to build the code. After building, the Unreal Engine editor will automatically open the environment. The editor will take several minutes to open and it will build shaders for 2min ~ 30min depending on the hardware used, but subsequent startups will be faster. If the editor fails to open or the build process fails, please check that the setup script ran successfully and did not generate errors.

If you wish to delete CiThruS2, simply delete the directory in which you downloaded this repository: the setup script does not install anything elsewhere on your computer.

## Paper

If you are using CiThruS2 in your research, please refer to the following paper: 

`E. Gałązka, T. T. Niemirepo, and J. Vanne, “CiThruS2: Open-source Photorealistic 3D Framework for Driving and Traffic Simulation in Real Time,” Accepted to IEEE Int. Conf. Intell. Transp., Indianapolis, Indiana, USA, Sept. 2021. `
