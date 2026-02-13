// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/**
 * HCIEditorGen 项目的 Game 编译目标设置。
 */
public class HCIEditorGenTarget : TargetRules
{
	public HCIEditorGenTarget(TargetInfo Target) : base(Target)
	{
		// 目标类型定义为独立游戏
		Type = TargetType.Game;
		// 设置默认编译设置版本
		DefaultBuildSettings = BuildSettingsVersion.V5;
		// 设置头文件包含顺序版本
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		// 添加主模块名称
		ExtraModuleNames.Add("HCIEditorGen");
	}
}
