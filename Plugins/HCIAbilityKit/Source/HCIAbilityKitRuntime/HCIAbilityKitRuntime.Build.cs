using UnrealBuildTool;

/**
 * HCIAbilityKit 插件的运行时模块构建规则喵
 * 该模块必须是纯净的，不能依赖任何 Editor 模块喵
 */
public class HCIAbilityKitRuntime : ModuleRules
{
	public HCIAbilityKitRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 运行时基础依赖项喵
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
				// 这里目前是空的喵
			});
	}
}
