using UnrealBuildTool;
using System.IO;

public class HCITests : ModuleRules
{
	public HCITests(ReadOnlyTargetRules Target) : base(Target)
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
				"HCIRuntime",
				"HCIEditor",
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
				Path.Combine(ModuleDirectory, "../HCIEditor/Private"),
				Path.Combine(ModuleDirectory, "../HCIRuntime/Private")
			});
	}
}
