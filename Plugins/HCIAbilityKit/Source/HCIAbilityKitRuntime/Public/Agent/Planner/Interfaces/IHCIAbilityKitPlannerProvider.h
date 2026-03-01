#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

#include "Agent/Planner/HCIAbilityKitAgentPlan.h"
#include "Agent/Planner/HCIAbilityKitAgentPlanner.h"

class FHCIAbilityKitToolRegistry;

class HCIABILITYKITRUNTIME_API IHCIAbilityKitPlannerProvider
{
public:
	virtual ~IHCIAbilityKitPlannerProvider() = default;

	virtual const TCHAR* GetName() const = 0;
	virtual bool IsNetworkBacked() const = 0;

	virtual bool BuildPlan(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		FHCIAbilityKitAgentPlan& OutPlan,
		FString& OutRouteReason,
		FHCIAbilityKitAgentPlannerResultMetadata& OutMetadata,
		FString& OutError) = 0;

	virtual void BuildPlanAsync(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		TFunction<void(bool, FHCIAbilityKitAgentPlan, FString, FHCIAbilityKitAgentPlannerResultMetadata, FString)>&& OnComplete) = 0;
};

