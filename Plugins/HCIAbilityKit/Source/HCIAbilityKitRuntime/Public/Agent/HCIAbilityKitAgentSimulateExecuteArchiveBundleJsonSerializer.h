#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentSimulateExecuteArchiveBundle;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentSimulateExecuteArchiveBundleJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAbilityKitAgentSimulateExecuteArchiveBundle& Bundle,
		FString& OutJsonText);
};
