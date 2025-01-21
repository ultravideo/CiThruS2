using UnrealBuildTool;
using System.IO;

public class CiThruS : ModuleRules
{
	public CiThruS(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("_WIN32_WINNT_WIN10_TH2");
        PublicDefinitions.Add("_WIN32_WINNT_WIN10_RS1");
        PublicDefinitions.Add("_WIN32_WINNT_WIN10_RS2");
        PublicDefinitions.Add("_WIN32_WINNT_WIN10_RS3");
        PublicDefinitions.Add("_WIN32_WINNT_WIN10_RS4");
        PublicDefinitions.Add("_WIN32_WINNT_WIN10_RS5");

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;
		bUseUnity = false;
		var uvgrtp_base_path = Path.Combine(ModuleDirectory, "../../ThirdParty/uvgRTP/");
		var kvazaar_base_path = Path.Combine(ModuleDirectory, "../../ThirdParty/Kvazaar/");
		
		// Keep these include paths even if the files don't exist because C++17 checks for them in VideoTransmitter.h at compilation time
		PublicIncludePaths.Add(Path.Combine(uvgrtp_base_path, "Include"));
		PublicIncludePaths.Add(Path.Combine(kvazaar_base_path, "Include"));

		if (File.Exists(Path.Combine(uvgrtp_base_path, "Lib/uvgrtp.lib")))
		{
			PublicAdditionalLibraries.Add(Path.Combine(uvgrtp_base_path, "Lib/uvgrtp.lib"));
		}

		if (File.Exists(Path.Combine(kvazaar_base_path, "Lib/kvazaar_lib.lib")))
		{
			PublicAdditionalLibraries.Add(Path.Combine(kvazaar_base_path, "Lib/kvazaar_lib.lib"));
		}
		
		// This is needed to use kvazaar.lib instead of kvazaar.dll
		PublicDefinitions.Add("KVZ_STATIC_LIB=1");
		
		// This lets us #include files with their path from the root folder which makes includes more convenient and consistent between files
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "RHI", "RenderCore", "AIModule", "Landscape", "ChaosVehicles", "Niagara", "NiagaraCore" });
	}
}
