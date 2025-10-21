using UnrealBuildTool;
using System.IO;

public class CiThruS : ModuleRules
{
	public CiThruS(ReadOnlyTargetRules Target) : base(Target)
	{
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // These are here to fix some weird Windows issue
            PublicDefinitions.Add("_WIN32_WINNT_WIN10_TH2");
            PublicDefinitions.Add("_WIN32_WINNT_WIN10_RS1");
            PublicDefinitions.Add("_WIN32_WINNT_WIN10_RS2");
            PublicDefinitions.Add("_WIN32_WINNT_WIN10_RS3");
            PublicDefinitions.Add("_WIN32_WINNT_WIN10_RS4");
            PublicDefinitions.Add("_WIN32_WINNT_WIN10_RS5");
        }

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;
		bUseUnity = false;
		
		var uvgrtp_base_path = Path.Combine(ModuleDirectory, "../../ThirdParty/uvgRTP/");
		var kvazaar_base_path = Path.Combine(ModuleDirectory, "../../ThirdParty/Kvazaar/");
		var openhevc_base_path = Path.Combine(ModuleDirectory, "../../ThirdParty/OpenHEVC/");
		var fpng_base_path = Path.Combine(ModuleDirectory, "../../ThirdParty/fpng/");

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            if (File.Exists(Path.Combine(uvgrtp_base_path, "Lib/uvgrtp.lib")))
            {
                PublicAdditionalLibraries.Add(Path.Combine(uvgrtp_base_path, "Lib/uvgrtp.lib"));
            }

            if (File.Exists(Path.Combine(kvazaar_base_path, "Lib/kvazaar_lib.lib")))
            {
                PublicAdditionalLibraries.Add(Path.Combine(kvazaar_base_path, "Lib/kvazaar_lib.lib"));
            }

            if (File.Exists(Path.Combine(openhevc_base_path, "Lib/LibOpenHevcWrapper.lib")))
            {
                PublicAdditionalLibraries.Add(Path.Combine(openhevc_base_path, "Lib/LibOpenHevcWrapper.lib"));
            }

            if (File.Exists(Path.Combine(fpng_base_path, "Lib/fpng.lib")))
            {
                PublicAdditionalLibraries.Add(Path.Combine(fpng_base_path, "Lib/fpng.lib"));
            }
        }

        if (Target.Platform == UnrealTargetPlatform.Linux)
		{
            if (File.Exists(Path.Combine(uvgrtp_base_path, "Lib/libuvgrtp.a")))
            {
                PublicAdditionalLibraries.Add(Path.Combine(uvgrtp_base_path, "Lib/libuvgrtp.a"));
            }

            if (File.Exists(Path.Combine(kvazaar_base_path, "Lib/libkvazaar.a")))
            {
                PublicAdditionalLibraries.Add(Path.Combine(kvazaar_base_path, "Lib/libkvazaar.a"));
            }

            if (File.Exists(Path.Combine(openhevc_base_path, "Lib/libLibOpenHevcWrapper.a")))
            {
                PublicAdditionalLibraries.Add(Path.Combine(openhevc_base_path, "Lib/libLibOpenHevcWrapper.a"));
            }

            if (File.Exists(Path.Combine(fpng_base_path, "Lib/fpng.a")))
            {
                PublicAdditionalLibraries.Add(Path.Combine(fpng_base_path, "Lib/fpng.a"));
            }
        }
		
		// These are needed to use static libraries
		PublicDefinitions.Add("KVZ_STATIC_LIB=1");
		
		// This lets us #include files with their path from the root folder which makes includes more convenient and consistent between files
		PublicIncludePaths.Add(ModuleDirectory);
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "../../ThirdParty/"));

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "RHI", "RenderCore", "AIModule", "Landscape", "ChaosVehicles", "Niagara", "NiagaraCore" });
		
		if (Target.bBuildEditor)
		{
            PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd" });
        }
	}
}
