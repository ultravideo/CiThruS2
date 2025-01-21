using UnrealBuildTool;
using System.Collections.Generic;

public class CiThruSEditorTarget : TargetRules
{
	public CiThruSEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;

		bUseUnityBuild = true;

        ExtraModuleNames.AddRange( new string[] { "CiThruS" } );
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
	}
}
