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
		
		var thirdparty_directory = Path.Combine(ModuleDirectory, "../../ThirdParty/");
		var engine_thirdparty_directory = Path.Combine(ModuleDirectory, "Binaries/ThirdParty/");
		
		var uvgrtp_base_path = Path.Combine(thirdparty_directory, "uvgRTP/");
		var kvazaar_base_path = Path.Combine(thirdparty_directory, "Kvazaar/");
		var openhevc_base_path = Path.Combine(thirdparty_directory, "OpenHEVC/");
		var fpng_base_path = Path.Combine(thirdparty_directory, "fpng/");
		var pahocpp_base_path = Path.Combine(thirdparty_directory, "PahoCpp/");
		
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
			AddStaticLibraryIfExists(Path.Combine(uvgrtp_base_path, "Lib/uvgrtp.lib"));
			AddStaticLibraryIfExists(Path.Combine(kvazaar_base_path, "Lib/kvazaar_lib.lib"));
			AddStaticLibraryIfExists(Path.Combine(openhevc_base_path, "Lib/LibOpenHevcWrapper.lib"));
			AddStaticLibraryIfExists(Path.Combine(fpng_base_path, "Lib/fpng.lib"));
			AddStaticLibraryIfExists(Path.Combine(pahocpp_base_path, "Lib/paho-mqttpp3-static.lib"));
			AddStaticLibraryIfExists(Path.Combine(pahocpp_base_path, "Lib/paho-mqtt3as-static.lib"));
            AddStaticLibraryIfExists(Path.Combine(pahocpp_base_path, "Lib/libssl.lib"));
            AddStaticLibraryIfExists(Path.Combine(pahocpp_base_path, "Lib/libcrypto.lib"));
        }

        if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			AddStaticLibraryIfExists(Path.Combine(uvgrtp_base_path, "Lib/libuvgrtp.a"));
			AddStaticLibraryIfExists(Path.Combine(kvazaar_base_path, "Lib/libkvazaar.a"));
			AddStaticLibraryIfExists(Path.Combine(openhevc_base_path, "Lib/libLibOpenHevcWrapper.a"));
			AddStaticLibraryIfExists(Path.Combine(fpng_base_path, "Lib/fpng.a"));
			AddStaticLibraryIfExists(Path.Combine(pahocpp_base_path, "Lib/libpaho-mqttpp3.a"));
			AddStaticLibraryIfExists(Path.Combine(pahocpp_base_path, "Lib/libpaho-mqtt3as.a"));
			PublicDependencyModuleNames.Add("OpenSSL");
        }
		
		// These are needed to use static libraries
		PublicDefinitions.Add("KVZ_STATIC_LIB=1");
		
		// This lets us #include files with their path from the root folder which makes includes more convenient and consistent between files
		PublicIncludePaths.Add(ModuleDirectory);
		PublicIncludePaths.Add(thirdparty_directory);
		
		PublicIncludePaths.Add(Path.Combine(pahocpp_base_path, "Include/"));

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "RHI", "RenderCore", "AIModule", "Landscape", "ChaosVehicles", "Niagara", "NiagaraCore" });
		
		if (Target.bBuildEditor)
		{
            PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd" });
        }
	}
	
	private void AddStaticLibraryIfExists(string libraryPath)
	{
		if (File.Exists(libraryPath))
		{
			PublicAdditionalLibraries.Add(libraryPath);
		}
	}

	private void AddSharedLibraryIfExists(string libraryPath)
	{
		if (File.Exists(libraryPath))
		{
			string filePath = libraryPath;

			if (filePath.Contains(".so"))
			{
				// This has to be done like this because if you pass a library like
				// libfoo.so.1.0 to this function, Unreal Engine thinks the file
				// extension is .0 instead of .so.1.0 and can't pass it to the linker
				filePath = filePath.Substring(0, filePath.IndexOf(".so") + 3);
			}

			PublicAdditionalLibraries.Add(filePath);
			RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)/", Path.GetFileName(libraryPath)), libraryPath);
		}
	}
}
