#include "HCIAbilityKitRuntimeModule.h"

#include "Modules/ModuleManager.h"

void FHCIAbilityKitRuntimeModule::StartupModule()
{
	// 当模块加载到内存时调用此函数喵。
	// 这里可以放置全局系统的初始化代码，比如注册运行时组件或配置喵。
}

void FHCIAbilityKitRuntimeModule::ShutdownModule()
{
	// 当模块从内存卸载时调用此函数喵。
	// 负责清理 StartupModule 中创建的所有资源，防止内存泄漏喵。
}

// 宏定义：将此模块导出给虚幻引擎的模块管理器喵
IMPLEMENT_MODULE(FHCIAbilityKitRuntimeModule, HCIAbilityKitRuntime)
