using UnrealBuildTool;
using System.IO;

public class HCIAbilityKitTests : ModuleRules
{
	public HCIAbilityKitTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"HCIAbilityKitRuntime",
				"HCIAbilityKitEditor",
				"UnrealEd",
				"Slate",
				"SlateCore",
				"AssetRegistry",
				"AssetTools",
				"ContentBrowser",
				"EditorScriptingUtilities",
				"Json",
				"JsonUtilities",
				"HTTP",
				"Projects",
				"PythonScriptPlugin"
			});

		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(ModuleDirectory, "../HCIAbilityKitEditor/Private"),
				Path.Combine(ModuleDirectory, "../HCIAbilityKitRuntime/Private")
			});
	}
}