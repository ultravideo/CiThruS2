#include "CiThruS.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FCiThruSModule, CiThruS, "CiThruS" );

void FCiThruSModule::StartupModule()
{
	AddShaderSourceDirectoryMapping("/Shaders", FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/CiThruS/Shaders")));
}
