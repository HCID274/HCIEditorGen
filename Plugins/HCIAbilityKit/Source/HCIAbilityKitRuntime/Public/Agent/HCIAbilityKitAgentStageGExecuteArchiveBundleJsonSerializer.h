#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentStageGExecuteArchiveBundle;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentStageGExecuteArchiveBundleJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAbilityKitAgentStageGExecuteArchiveBundle& Receipt,
		FString& OutJsonText);
};

