#include "HCIAbilityKitRuntimeModule.h"

#include "Modules/ModuleManager.h"

void FHCIAbilityKitRuntimeModule::StartupModule()
{
	// 当模块加载到内存时调用此函数。
	// 这里可以放置全局系统的初始化代码，例如注册运行时组件或配置。
}

void FHCIAbilityKitRuntimeModule::ShutdownModule()
{
	// 当模块从内存卸载时调用此函数。
	// 负责清理 StartupModule 中创建的所有资源，防止内存泄漏。
}

// 宏定义：将此模块导出给虚幻引擎的模块管理器
IMPLEMENT_MODULE(FHCIAbilityKitRuntimeModule, HCIAbilityKitRuntime)
