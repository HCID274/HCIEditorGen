using UnrealBuildTool;

public class HCIAbilityKitEditor : ModuleRules
{
	public HCIAbilityKitEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"HCIAbilityKitRuntime"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"ContentBrowser",
				"Json"
			});
	}
}
