using UnrealBuildTool;

// Parser fuer die Freelancer-Originalformate (UTF, INI, VMesh). Nur im Editor (Import), nie zur Laufzeit.
// Reines C++/STL, wird auch von Tools/fltool (CMake) gebaut.
public class FreelancerFormats : ModuleRules
{
	public FreelancerFormats(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;
		bEnableExceptions = false;
		ShadowVariableWarningLevel = WarningLevel.Off;
		PublicDependencyModuleNames.Add("Core");
	}
}
