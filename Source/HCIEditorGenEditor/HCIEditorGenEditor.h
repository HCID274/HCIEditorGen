#pragma once
#include "Modules/ModuleManager.h"

/**
 * 项目级别的主 Editor 模块喵
 * 用于处理不属于特定插件的编辑器扩展逻辑喵。
 */
class FHCIEditorGenEditorModule : public IModuleInterface{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};