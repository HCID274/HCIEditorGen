#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentStageGExecuteIntent;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentStageGExecuteIntentJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAbilityKitAgentStageGExecuteIntent& Intent,
		FString& OutJsonText);
};


