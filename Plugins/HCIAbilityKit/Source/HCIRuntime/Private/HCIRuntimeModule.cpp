#include "HCIRuntimeModule.h"

#include "Agent/Planner/Interfaces/IHCIPlannerRouter.h"
#include "Agent/Planner/Providers/HCIKeywordPlannerProvider.h"
#include "Agent/Planner/Providers/HCILlmPlannerProvider.h"
#include "Agent/Planner/Router/HCIPlannerRouter_Default.h"

#include "Modules/ModuleManager.h"

FHCIRuntimeModule& FHCIRuntimeModule::Get()
{
	return FModuleManager::LoadModuleChecked<FHCIRuntimeModule>(TEXT("HCIRuntime"));
}

void FHCIRuntimeModule::StartupModule()
{
	// 当模块加载到内存时调用此函数。
	// 这里可以放置全局系统的初始化代码，例如注册运行时组件或配置。
}

void FHCIRuntimeModule::ShutdownModule()
{
	FScopeLock Lock(&PlannerRouterMutex);
	PlannerRouter.Reset();

	// 当模块从内存卸载时调用此函数。
	// 负责清理 StartupModule 中创建的所有资源，防止内存泄漏。
}

void FHCIRuntimeModule::RegisterPlannerRouter(const TSharedRef<IHCIPlannerRouter>& InRouter)
{
	FScopeLock Lock(&PlannerRouterMutex);
	PlannerRouter = InRouter;
}

TSharedRef<IHCIPlannerRouter> FHCIRuntimeModule::GetPlannerRouter()
{
	{
		FScopeLock Lock(&PlannerRouterMutex);
		if (PlannerRouter.IsValid())
		{
			return PlannerRouter.ToSharedRef();
		}
	}

	TSharedRef<IHCIPlannerRouter> DefaultRouter = CreateDefaultPlannerRouter();
	RegisterPlannerRouter(DefaultRouter);
	return DefaultRouter;
}

TSharedRef<IHCIPlannerRouter> FHCIRuntimeModule::CreateDefaultPlannerRouter()
{
	TSharedRef<IHCIPlannerProvider> LlmProvider = MakeShared<FHCILlmPlannerProvider>();
	TSharedRef<IHCIPlannerProvider> KeywordProvider = MakeShared<FHCIKeywordPlannerProvider>();
	return MakeShared<FHCIPlannerRouter_Default>(LlmProvider, KeywordProvider);
}

void FHCIRuntimeModule::ResetPlannerRouterForTesting()
{
	RegisterPlannerRouter(CreateDefaultPlannerRouter());
}

// 宏定义：将此模块导出给虚幻引擎的模块管理器
IMPLEMENT_MODULE(FHCIRuntimeModule, HCIRuntime)

