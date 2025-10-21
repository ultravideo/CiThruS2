using UnrealBuildTool;

public class CiThruSEditorTarget : TargetRules
{
	public CiThruSEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;

		bUseUnityBuild = true;

        ExtraModuleNames.AddRange( new string[] { "CiThruS" } );
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		if (Platform == UnrealTargetPlatform.Linux)
		{
            // These are needed to link static libraries on Linux
            AdditionalLinkerArguments += " -Wl,-Bsymbolic";
            bOverrideBuildEnvironment = true;
        }
	}
}
