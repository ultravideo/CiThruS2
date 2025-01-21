using UnrealBuildTool;
using System.Collections.Generic;

public class CiThruSTarget : TargetRules
{
	public CiThruSTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;

        bUseUnityBuild = false;

        ExtraModuleNames.AddRange( new string[] { "CiThruS" } );
	}
}
