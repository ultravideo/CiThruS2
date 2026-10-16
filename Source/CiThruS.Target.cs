using UnrealBuildTool;

public class CiThruSTarget : TargetRules
{
	public CiThruSTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;

		bUseUnityBuild = false;

        ExtraModuleNames.AddRange(new string[] { "CiThruS" });
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        string projectRootPath = ProjectFile.ToNormalizedPath();
        projectRootPath = projectRootPath.Substring(0, projectRootPath.LastIndexOf('/') + 1);
        string fastDdsPath = projectRootPath + "ThirdParty/FastDDS/";
        string fastDdsGenPath = "\"" + projectRootPath + "ThirdParty/FastDDS/Bin/fastddsgen.jar\"";
        string idlFilesDir = projectRootPath + "Source/CiThruS/PubSub/DdsMessages";

        string[] fastDdsGeneratedFileNames
            = new string[]
            {
                ".hpp",
                "PubSubTypes.hpp",
                "PubSubTypes.cxx",
                "TypeObjectSupport.hpp",
                "TypeObjectSupport.cxx",
                "CdrAux.hpp",
                "CdrAux.ipp"
            };

        if (Platform == UnrealTargetPlatform.Win64)
        {
            // Some Windows tools only understand backslashes
            fastDdsPath = fastDdsPath.Replace("/", "\\");
            fastDdsGenPath = fastDdsGenPath.Replace("/", "\\");
            idlFilesDir = idlFilesDir.Replace("/", "\\");

            string filesToCheck = "";

            foreach (string fileName in fastDdsGeneratedFileNames)
            {
                filesToCheck += " \"%%~dpnf" + fileName + "\"";
            }

            PreBuildSteps.Add("setlocal enabledelayedexpansion\r\n"
                + "set DDS_GEN_EXISTS=0\r\n"
                + "if exist " + fastDdsGenPath + " set DDS_GEN_EXISTS=1\r\n"
                // Delete generated files if they are older than the corresponding .idl file
                // or Fast DDS is disabled
                + "for %%f in (\"" + idlFilesDir + "\\*.idl\") do (\r\n"
                + "    set NEEDS_GENERATION=0\r\n"
                + "    for %%g in (" + filesToCheck + ") do (\r\n"
                + "        if exist \"%%~fg\" (\r\n"
                + "            set DELETE_THIS=0\r\n"
                + "            if %DDS_GEN_EXISTS%==0 set DELETE_THIS=1\r\n"
                               // This seems to be the only way to check if a file is newer in a batch file...
                + "            xcopy /l /d /y \"%%~ff\" \"%%~fg\" 2>nul | findstr /b /c:\"1 File\" >nul && set DELETE_THIS=1\r\n"
                + "            if !DELETE_THIS!==1 del /q \"%%~fg\"))\r\n"
                + "    for %%g in (" + filesToCheck + ") do (\r\n"
                + "        if not exist \"%%~fg\" set NEEDS_GENERATION=1)\r\n"
                + "    if !NEEDS_GENERATION!==1 set \"IDL_FILES=!IDL_FILES! \"%%~ff\"\")\r\n"
                // If fastddsgen.jar exists, execute it for all outdated .idl files in idlFilesDir
                + "if %DDS_GEN_EXISTS%==0 exit /b 0\r\n"
                + "if \"%IDL_FILES%\"==\"\" exit /b 0\r\n"
                // Fast DDS-Gen uses cl.exe for some reason...
                + "where cl.exe 2>&1>nul\r\n"
                + "if errorlevel 1 (\r\n"
                + "    set VCVARS_CALLED=0\r\n"
                + "    for /d %%i in (\"%ProgramW6432%\\Microsoft Visual Studio\\*\") do (\r\n"
                + "        for /d %%j in (\"%%i\\*\") do (\r\n"
                + "            if !VCVARS_CALLED!==0 call \"%%j\\VC\\Auxiliary\\Build\\vcvars64.bat\" 2>nul\r\n"
                + "            if errorlevel 0 (\r\n"
                + "                set VCVARS_CALLED=1)))\r\n"
                + "    where cl.exe 2>&1>nul\r\n"
                + "    if errorlevel 1 exit /b 0)\r\n"
                + "\"" + fastDdsPath + "Java\\bin\\java.exe\" -jar " + fastDdsGenPath
                + " -typeros2 -cs -d \"" + idlFilesDir + "\" %IDL_FILES%");
        }

        if (Platform == UnrealTargetPlatform.Linux)
        {
            string filesToCheck = "";

            foreach (string fileName in fastDdsGeneratedFileNames)
            {
                filesToCheck += " \"${idlFile%.*}\"" + fileName;
            }

            PreBuildSteps.Add("DDS_GEN_EXISTS=0\n"
                + "[ -f " + fastDdsGenPath + " ] && DDS_GEN_EXISTS=1\n"
                // Delete generated files if they are older than the corresponding .idl file
                // or Fast DDS is disabled
                + "for idlFile in \"" + idlFilesDir + "/\"*.idl; do\n"
                + "    NEEDS_GENERATION=0\n"
                + "    for generatedFile in " + filesToCheck + "; do\n"
                + "        if [ -f \"${generatedFile}\" ] && [ ${DDS_GEN_EXISTS} = 0 ] || [ \"${generatedFile}\" -ot \"${idlFile}\" ]; then\n"
                + "            rm \"${generatedFile}\"\n"
                + "        fi\n"
                + "    done\n"
                + "    for generatedFile in " + filesToCheck + "; do\n"
                + "        if [ ! -f \"${generatedFile}\" ]; then\n"
                + "            NEEDS_GENERATION=1\n"
                + "        fi\n"
                + "    done\n"
                + "    if [ ${NEEDS_GENERATION} = 1 ]; then\n"
                + "        IDL_FILES=\"${IDL_FILES} ${idlFile}\"\n"
                + "    fi\n"
                + "done\n"
                // If fastddsgen.jar exists, execute it for all outdated .idl files in idlFilesDir
                + "if [ ${DDS_GEN_EXISTS} = 0 ] || [ \"${IDL_FILES}\" = \"\" ]; then\n"
                + "    exit 0\n"
                + "fi\n"
                + "\"" + fastDdsPath + "Java/bin/java\" -jar " + fastDdsGenPath
                + " -typeros2 -cs -d \"" + idlFilesDir + "\" ${IDL_FILES}\n");

            // These are needed to link static libraries on Linux
            AdditionalLinkerArguments += " -Wl,-Bsymbolic";
            bOverrideBuildEnvironment = true;
        }
    }
}
