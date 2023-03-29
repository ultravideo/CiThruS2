// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.IO;

public class HervantaUE4 : ModuleRules
{
	public HervantaUE4(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		var uvgrtp_base_path = Path.Combine(ModuleDirectory, "../../ThirdParty/uvgRTP/");
		var kvazaar_base_path = Path.Combine(ModuleDirectory, "../../ThirdParty/Kvazaar/");

		PublicIncludePaths.Add(Path.Combine(uvgrtp_base_path, "Include"));
		PublicIncludePaths.Add(Path.Combine(kvazaar_base_path, "Include"));

		PublicAdditionalLibraries.Add(Path.Combine(uvgrtp_base_path, "Lib/uvgrtp.lib"));
		PublicAdditionalLibraries.Add(Path.Combine(kvazaar_base_path, "Lib/kvazaar_lib.lib"));
		
		Definitions.Add("KVZ_STATIC_LIB=1");
		//Definitions.Add("CITHRUS_NO_KVAZAAR_OR_UVGRTP=1");

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "RHI", "RenderCore", "AIModule" });
	}
}
