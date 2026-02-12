#pragma once

#include "Modules/ModuleManager.h"

/**
 * HCIAbilityKit Editor 模块类
 * 负责插件在编辑器环境下的交互逻辑、菜单扩展等功能喵。
 */
class FHCIAbilityKitEditorModule : public IModuleInterface
{
public:
	/** 模块启动时的回调函数，通常用于初始化单例、注册菜单等喵 */
	virtual void StartupModule() override;
	/** 模块卸载时的回调函数，用于清理资源喵 */
	virtual void ShutdownModule() override;
};
