using UnrealBuildTool;

// Editor-Modul: Import der Freelancer-Originaldaten (Commandlet FLImport).
public class FreelancerEditor : ModuleRules
{
	public FreelancerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "UnrealEd", "AssetTools", "Freelancer", "FreelancerFormats", "MaterialEditor", "MeshDescription", "StaticMeshDescription" });
	}
}
