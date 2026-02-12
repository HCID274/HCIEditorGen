// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/**
 * HCIEditorGen 项目的 Game 编译目标设置喵
 */
public class HCIEditorGenTarget : TargetRules
{
	public HCIEditorGenTarget(TargetInfo Target) : base(Target)
	{
		// 目标类型为游戏喵
		Type = TargetType.Game;
		// 使用最新的编译设置版本喵
		DefaultBuildSettings = BuildSettingsVersion.V5;
		// 包含顺序版本设置为 5.4 喵
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		// 添加主游戏模块名喵
		ExtraModuleNames.Add("HCIEditorGen");
	}
}
