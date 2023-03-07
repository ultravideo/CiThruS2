// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.IO;

public class HervantaUE4 : ModuleRules
{
	public HervantaUE4(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		var uvgrtp_base_path = Path.Combine(ModuleDirectory, "../../Submodules/uvgrtp/");
		var kvazaar_base_path = Path.Combine(ModuleDirectory, "../../Submodules/kvazaar/");

		PublicIncludePaths.Add(Path.Combine(uvgrtp_base_path, "include"));
		PublicIncludePaths.Add(Path.Combine(kvazaar_base_path, "src"));

		PublicAdditionalLibraries.Add(Path.Combine(uvgrtp_base_path, "build/Release/uvgrtp.lib"));
		PublicAdditionalLibraries.Add(Path.Combine(kvazaar_base_path, "build/x64-Release-libs/kvazaar_lib.lib"));
		
		Definitions.Add("KVZ_STATIC_LIB=1");

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "RHI", "RenderCore", "AIModule" });
	}
}
