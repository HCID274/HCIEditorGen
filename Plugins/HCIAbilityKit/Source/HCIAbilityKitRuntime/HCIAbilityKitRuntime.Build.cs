using UnrealBuildTool;

/**
 * HCIAbilityKit 插件的运行时模块构建规则。
 * 包含资产定义与核心解析服务，不应依赖任何编辑器模块。
 */
public class HCIAbilityKitRuntime : ModuleRules
{
	public HCIAbilityKitRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 运行时基础框架依赖项
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			});

		// 私有 JSON 处理依赖
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Json"
			});
	}
}
