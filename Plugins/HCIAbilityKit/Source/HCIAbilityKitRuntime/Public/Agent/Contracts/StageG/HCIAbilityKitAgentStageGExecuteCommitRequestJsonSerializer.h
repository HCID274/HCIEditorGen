#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentStageGExecuteCommitRequest;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentStageGExecuteCommitRequestJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAbilityKitAgentStageGExecuteCommitRequest& Request,
		FString& OutJsonText);
};




