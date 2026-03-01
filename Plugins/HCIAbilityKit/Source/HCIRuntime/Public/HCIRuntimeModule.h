#pragma once

#include "HAL/CriticalSection.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"

class IHCIPlannerRouter;

/**
 * HCI Runtime 模块类
 * 负责插件运行时核心逻辑的加载与卸载。
 */
class HCIRUNTIME_API FHCIRuntimeModule : public IModuleInterface
{
public:
	static FHCIRuntimeModule& Get();

	/** 模块启动时的回调函数 */
	virtual void StartupModule() override;
	/** 模块卸载时的回调函数 */
	virtual void ShutdownModule() override;

	/**
	 * Register a planner router instance (dependency injection point).
	 * Editor module is allowed to override the default router during startup.
	 */
	void RegisterPlannerRouter(const TSharedRef<IHCIPlannerRouter>& InRouter);

	/** Get the active planner router (will lazily create a default router if missing). */
	TSharedRef<IHCIPlannerRouter> GetPlannerRouter();

	/** Create the default router wired with default providers (no statics; caller may register it). */
	TSharedRef<IHCIPlannerRouter> CreateDefaultPlannerRouter();

	/** Testing hook (resets router state by recreating the default router). */
	void ResetPlannerRouterForTesting();

private:
	FCriticalSection PlannerRouterMutex;
	TSharedPtr<IHCIPlannerRouter> PlannerRouter;
};


