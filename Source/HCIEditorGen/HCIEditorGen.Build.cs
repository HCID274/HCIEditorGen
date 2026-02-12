// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/**
 * HCIEditorGen 项目主游戏模块的构建规则喵
 */
public class HCIEditorGen : ModuleRules
{
	public HCIEditorGen(ReadOnlyTargetRules Target) : base(Target)
	{
		// 使用显式或共享 PCH（预编译头），可以加速编译喵
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		// 公共依赖项，这些模块的功能可以在头文件中公开使用喵
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core",             // 基础引擎功能：字符串、集合、数学库等喵
			"CoreUObject",      // UObject 系统：反射、垃圾回收等喵
			"Engine"            // 引擎核心：Actor、组件、关卡逻辑等喵
		});

		// 私有依赖项，仅在 .cpp 中使用的模块喵
		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
