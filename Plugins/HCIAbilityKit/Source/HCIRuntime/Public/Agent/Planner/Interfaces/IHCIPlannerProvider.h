#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

#include "Agent/Planner/HCIAgentPlan.h"
#include "Agent/Planner/HCIAgentPlanner.h"

class FHCIToolRegistry;

class HCIRUNTIME_API IHCIPlannerProvider
{
public:
	virtual ~IHCIPlannerProvider() = default;

	virtual const TCHAR* GetName() const = 0;
	virtual bool IsNetworkBacked() const = 0;

	virtual bool BuildPlan(
		const FString& UserText,
		const FString& RequestId,
		const FHCIToolRegistry& ToolRegistry,
		const FHCIAgentPlannerBuildOptions& Options,
		FHCIAgentPlan& OutPlan,
		FString& OutRouteReason,
		FHCIAgentPlannerResultMetadata& OutMetadata,
		FString& OutError) = 0;

	virtual void BuildPlanAsync(
		const FString& UserText,
		const FString& RequestId,
		const FHCIToolRegistry& ToolRegistry,
		const FHCIAgentPlannerBuildOptions& Options,
		TFunction<void(bool, FHCIAgentPlan, FString, FHCIAgentPlannerResultMetadata, FString)>&& OnComplete) = 0;
};



