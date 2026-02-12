// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/**
 * HCIEditorGen 项目的 Editor 编译目标设置喵
 */
public class HCIEditorGenEditorTarget : TargetRules
{
	public HCIEditorGenEditorTarget( TargetInfo Target) : base(Target)
	{
		// 目标类型为编辑器喵
		Type = TargetType.Editor;
		// 使用最新的编译设置版本喵
		DefaultBuildSettings = BuildSettingsVersion.V5;
		// 包含顺序版本设置为 5.4 喵
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		// Editor Target 仅装载项目运行时模块，编辑器能力由插件模块提供
		ExtraModuleNames.AddRange(new string[]{"HCIEditorGen"});
	}
}
