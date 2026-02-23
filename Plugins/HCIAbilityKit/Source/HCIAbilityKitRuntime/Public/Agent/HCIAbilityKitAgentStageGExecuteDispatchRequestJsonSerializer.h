#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentStageGExecuteDispatchRequest;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentStageGExecuteDispatchRequestJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAbilityKitAgentStageGExecuteDispatchRequest& Request,
		FString& OutJsonText);
};


