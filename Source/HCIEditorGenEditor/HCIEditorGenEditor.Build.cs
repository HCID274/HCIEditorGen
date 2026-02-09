using UnrealBuildTool;

/// <summary>
/// UBT 你好，我是 HCIEditorGenEditor 模块。
/// 为了干活，我需要用到基础引擎功能（Core/Engine），
/// 我还需要认识隔壁的游戏数据（HCIEditorGen），
/// 最重要的是，我需要编辑器的特权（UnrealEd）和画界面的能力（Slate）。
/// 请把这些库都链接给我，谢谢！
/// </summary>
public class HCIEditorGenEditor : ModuleRules // 模块规则，每一个文件夹下都时一个Module，必须有一个 .Build.cs 文件
{
	public HCIEditorGenEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;// 加速编译
		
		//公开依赖
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",//引擎最底层的库，包括基础数据类型（int32,FString）/日志（UE_LOG）/数学库等
			"CoreUObject",//虚幻核心对象系统。包括UObject/UClass(反射系统)，垃圾回收（GC）
			"Engine",//游戏引擎主体，包括Actor,Component,World等概念
			"HCIEditorGen"//哥们自己写的Runtime模块
		});
		
		//私有依赖--编辑器特权套餐，Runtime模块绝对不能加这些，否则打包会挂
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",//编辑器大脑 包含所有编辑器独有功能：资源导入管线（Factory）/右键菜单扩展/Content Browser 的操作逻辑
			
			//虚幻的UI框架，编辑器界面时用Slate写的
			"Slate",
			"SlateCore",
			"Json",
			"ContentBrowser",
			"ToolMenus"
		});
	}
}
