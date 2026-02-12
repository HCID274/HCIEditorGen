#pragma once

#include "Modules/ModuleManager.h"

/**
 * HCIAbilityKit Runtime 模块类
 * 负责插件运行时核心逻辑的加载与卸载喵。
 */
class FHCIAbilityKitRuntimeModule : public IModuleInterface
{
public:
	/** 模块启动时的回调函数喵 */
	virtual void StartupModule() override;
	/** 模块卸载时的回调函数喵 */
	virtual void ShutdownModule() override;
};
