#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentPlan;
class FHCIAbilityKitToolRegistry;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlanner
{
public:
	static bool BuildPlanFromNaturalLanguage(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		FHCIAbilityKitAgentPlan& OutPlan,
		FString& OutRouteReason,
		FString& OutError);
};

