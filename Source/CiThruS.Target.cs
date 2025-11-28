using UnrealBuildTool;

public class CiThruSTarget : TargetRules
{
	public CiThruSTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;

        bUseUnityBuild = false;

		ExtraModuleNames.AddRange(new string[] { "CiThruS" });

        if (Platform == UnrealTargetPlatform.Linux)
        {
            // These are needed to link static libraries on Linux
            AdditionalLinkerArguments += " -Wl,-Bsymbolic";
            bOverrideBuildEnvironment = true;
        }
    }
}
