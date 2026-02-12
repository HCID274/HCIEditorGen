using UnrealBuildTool;

/**
 * HCIAbilityKit 插件的编辑器模块构建规则喵
 * 该模块包含了工厂、菜单扩展等编辑器专用功能喵
 */
public class HCIAbilityKitEditor : ModuleRules
{
	public HCIAbilityKitEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 公共依赖项，这里包含对 Runtime 模块的依赖喵
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"HCIAbilityKitRuntime" // 依赖运行时模块以使用其定义的资产类型喵
			});

		// 编辑器专属依赖项，这些模块在打包后的游戏中不可用喵
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",         // 编辑器核心逻辑喵
				"Slate",            // UI 框架喵
				"SlateCore",        // UI 核心喵
				"ToolMenus",        // 菜单扩展喵
				"ContentBrowser",   // 内容浏览器扩展喵
				"Json"              // 用于解析导入文件的 JSON 数据喵
			});
	}
}
