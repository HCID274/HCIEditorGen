#include "HCIAbilityKitRuntimeModule.h"

#include "Agent/Planner/Interfaces/IHCIAbilityKitPlannerRouter.h"
#include "Agent/Planner/Providers/HCIAbilityKitKeywordPlannerProvider.h"
#include "Agent/Planner/Providers/HCIAbilityKitLlmPlannerProvider.h"
#include "Agent/Planner/Router/HCIAbilityKitPlannerRouter_Default.h"

#include "Modules/ModuleManager.h"

FHCIAbilityKitRuntimeModule& FHCIAbilityKitRuntimeModule::Get()
{
	return FModuleManager::LoadModuleChecked<FHCIAbilityKitRuntimeModule>(TEXT("HCIAbilityKitRuntime"));
}

void FHCIAbilityKitRuntimeModule::StartupModule()
{
	// 当模块加载到内存时调用此函数。
	// 这里可以放置全局系统的初始化代码，例如注册运行时组件或配置。
}

void FHCIAbilityKitRuntimeModule::ShutdownModule()
{
	FScopeLock Lock(&PlannerRouterMutex);
	PlannerRouter.Reset();

	// 当模块从内存卸载时调用此函数。
	// 负责清理 StartupModule 中创建的所有资源，防止内存泄漏。
}

void FHCIAbilityKitRuntimeModule::RegisterPlannerRouter(const TSharedRef<IHCIAbilityKitPlannerRouter>& InRouter)
{
	FScopeLock Lock(&PlannerRouterMutex);
	PlannerRouter = InRouter;
}

TSharedRef<IHCIAbilityKitPlannerRouter> FHCIAbilityKitRuntimeModule::GetPlannerRouter()
{
	{
		FScopeLock Lock(&PlannerRouterMutex);
		if (PlannerRouter.IsValid())
		{
			return PlannerRouter.ToSharedRef();
		}
	}

	TSharedRef<IHCIAbilityKitPlannerRouter> DefaultRouter = CreateDefaultPlannerRouter();
	RegisterPlannerRouter(DefaultRouter);
	return DefaultRouter;
}

TSharedRef<IHCIAbilityKitPlannerRouter> FHCIAbilityKitRuntimeModule::CreateDefaultPlannerRouter()
{
	TSharedRef<IHCIAbilityKitPlannerProvider> LlmProvider = MakeShared<FHCIAbilityKitLlmPlannerProvider>();
	TSharedRef<IHCIAbilityKitPlannerProvider> KeywordProvider = MakeShared<FHCIAbilityKitKeywordPlannerProvider>();
	return MakeShared<FHCIAbilityKitPlannerRouter_Default>(LlmProvider, KeywordProvider);
}

void FHCIAbilityKitRuntimeModule::ResetPlannerRouterForTesting()
{
	RegisterPlannerRouter(CreateDefaultPlannerRouter());
}

// 宏定义：将此模块导出给虚幻引擎的模块管理器
IMPLEMENT_MODULE(FHCIAbilityKitRuntimeModule, HCIAbilityKitRuntime)
