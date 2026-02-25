#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentPlan;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentPlanJsonSerializer
{
public:
	static bool SerializeToJsonString(const FHCIAbilityKitAgentPlan& Plan, FString& OutJsonText);
};

