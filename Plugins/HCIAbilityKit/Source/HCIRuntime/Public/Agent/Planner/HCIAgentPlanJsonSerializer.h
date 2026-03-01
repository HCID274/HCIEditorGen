#pragma once

#include "CoreMinimal.h"

struct FHCIAgentPlan;

class HCIRUNTIME_API FHCIAgentPlanJsonSerializer
{
public:
	static bool SerializeToJsonString(const FHCIAgentPlan& Plan, FString& OutJsonText);
};



