#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentStageGWriteEnableRequest;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentStageGWriteEnableRequestJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAbilityKitAgentStageGWriteEnableRequest& Request,
		FString& OutJsonText);
};


