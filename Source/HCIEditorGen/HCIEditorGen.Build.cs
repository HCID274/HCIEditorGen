// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/**
 * HCIEditorGen 项目主游戏模块的构建规则。
 */
public class HCIEditorGen : ModuleRules
{
	public HCIEditorGen(ReadOnlyTargetRules Target) : base(Target)
	{
		// 使用显式或共享 PCH（预编译头），以加速编译过程
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		// 公共依赖项，这些模块的功能可以在公共头文件中引用
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core",             // 基础引擎功能：字符串、集合、数学库等
			"CoreUObject",      // UObject 系统：反射、垃圾回收等
			"Engine"            // 引擎核心：Actor、组件、关卡逻辑等
		});

		// 私有依赖项，仅在该模块内部的 .cpp 文件中使用
		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
