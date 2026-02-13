using UnrealBuildTool;

/**
 * HCIAbilityKit 插件的编辑器模块构建规则。
 * 包含资产工厂、菜单扩展以及 Python 脚本集成等编辑器专用逻辑。
 */
public class HCIAbilityKitEditor : ModuleRules
{
	public HCIAbilityKitEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		// 启用预编译头以优化编译速度
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 公共依赖项，包含对运行时模块的引用以访问其定义的资产类型
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"HCIAbilityKitRuntime" 
			});

		// 私有依赖项，仅在编辑器环境下可用
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",         // 编辑器核心逻辑
				"Slate",            // UI 系统
				"SlateCore",        // UI 核心
				"ToolMenus",        // 动态菜单系统
				"ContentBrowser",   // 内容浏览器扩展
				"Json",             // JSON 解析支持
				"PythonScriptPlugin" // Python 脚本执行支持
			});
	}
}
