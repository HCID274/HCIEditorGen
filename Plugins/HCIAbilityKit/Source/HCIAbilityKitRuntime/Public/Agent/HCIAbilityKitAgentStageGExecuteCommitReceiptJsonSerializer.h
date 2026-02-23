#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentStageGExecuteCommitReceipt;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentStageGExecuteCommitReceiptJsonSerializer
{
public:
	static bool SerializeToJsonString(
		const FHCIAbilityKitAgentStageGExecuteCommitReceipt& Receipt,
		FString& OutJsonText);
};

