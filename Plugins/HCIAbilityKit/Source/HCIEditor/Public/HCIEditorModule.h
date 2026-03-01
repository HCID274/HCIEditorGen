#pragma once

#include "Modules/ModuleManager.h"
#include "Templates/UniquePtr.h"

class FHCIContentBrowserMenuRegistrar;
class FHCIAgentDemoConsoleCommands;

/**
 * HCI Editor 模块类
 * 负责插件在编辑器环境下的交互逻辑、菜单扩展等功能。
 */
class FHCIEditorModule : public IModuleInterface
{
public:
	/** 模块启动时的回调函数，通常用于初始化单例、注册菜单等 */
	virtual void StartupModule() override;
	/** 模块卸载时的回调函数，用于清理资源 */
	virtual void ShutdownModule() override;

private:
	TUniquePtr<FHCIContentBrowserMenuRegistrar> ContentBrowserMenuRegistrar;
	TUniquePtr<FHCIAgentDemoConsoleCommands> AgentDemoConsoleCommands;
};

